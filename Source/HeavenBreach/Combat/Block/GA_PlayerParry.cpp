#include "Combat/Block/GA_PlayerParry.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Combat/Block/GA_GuardStance.h"
#include "Combat/Effect/HBEffectLibrary.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Animation/AnimMontage.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Equipment/EquipmentComponent.h"
#include "Player/HBPlayerCharacter.h"

UGA_PlayerParry::UGA_PlayerParry()
{
	// TryParry 가 이 태그로 발동시킨다. BP 가 이 태그를 잃으면 R 이 죽는다.
	SetAssetTags(FGameplayTagContainer(AbilityTag::TAG_Ability_Parry));

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);

	// 후딜 중에는 다시 패링할 수 없다. 이게 연타를 막는 유일한 장치다.
	//
	// 전용 태그를 쓴다. State.AfterDelay 를 빌려 쓰려다 말았는데,
	// 그건 몬스터 공격 후딜용이고(ST_EnemyAI 와 GE_*Delay 11개가 읽는다)
	// 플레이어 쪽에서 보는 코드가 하나도 없었다. 의미를 섞지 않는다.
	ActivationBlockedTags.AddTag(StateTag::TAG_State_ParryRecovery);

	// 방어태세 유지 중에도 재패링은 불가. 홀드한 채로 창이 반복되면
	// 방어태세가 곧 무한 패링이 된다.
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
}

void UGA_PlayerParry::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 파손된 방패로는 막을 수 없다. 기존 방어 규칙을 그대로 가져왔다.
	if (const UEquipmentComponent* Equipment = Player->GetEquipment())
	{
		if (Equipment->IsEquippedItemBroken(EEquipSlotType::SecondaryEquip))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 방패를 올리는 동작. 연출이라 없어도 판정은 그대로 돈다.
	//
	// 몽타주 완료를 기다리지 않는다. 판정의 주인은 창(0.2초)이고 애니메이션
	// 길이와 무관해야 한다. 어빌리티가 끝나면 태스크가 몽타주를 멈추므로
	// (bStopWhenAbilityEnds 기본값) 블렌드 아웃으로 자연스럽게 내려온다.
	//
	// 섹션을 지정하지 않으면 처음(Start)부터 재생된다. 그게 방패를 드는 동작이다.
	if (UAnimMontage* Montage = Player->GetGuardMontage())
	{
		if (UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, Montage, MontagePlayRate))
		{
			MontageTask->ReadyForActivation();
		}
	}

	// 창은 서버에서만 연다. 판정도 서버에서만 돌기 때문이다.
	if (ActorInfo->IsNetAuthority())
	{
		HBEffects::ApplyTaggedDuration(
			ActorInfo->AbilitySystemComponent.Get(), WindowDuration, StateTag::TAG_State_ParryWindow);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Parry] 창 열림 | Actor=%s Server=%d Duration=%.2f"),
		*GetNameSafe(Player), ActorInfo->IsNetAuthority() ? 1 : 0, WindowDuration);

	// 성립 신호를 기다린다. 창이 닫힐 때 후딜을 걸지 말지가 이걸로 갈린다.
	bParrySucceeded = false;

	UAbilityTask_WaitGameplayEvent* WaitSuccess = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EventTag::TAG_Event_Parry_Success, nullptr, false, false);
	WaitSuccess->EventReceived.AddDynamic(this, &UGA_PlayerParry::OnParrySuccess);
	WaitSuccess->ReadyForActivation();

	// R 을 뗐는지도 같이 본다. 창이 끝날 때 이 값으로 탭/홀드가 갈린다.
	bInputReleased = false;

	UAbilityTask_WaitGameplayEvent* WaitRelease = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EventTag::TAG_Event_Block_Release, nullptr, false, false);
	WaitRelease->EventReceived.AddDynamic(this, &UGA_PlayerParry::OnReleaseReceived);
	WaitRelease->ReadyForActivation();

	UAbilityTask_WaitDelay* WaitWindow = UAbilityTask_WaitDelay::WaitDelay(this, WindowDuration);
	WaitWindow->OnFinish.AddDynamic(this, &UGA_PlayerParry::OnWindowExpired);
	WaitWindow->ReadyForActivation();
}

void UGA_PlayerParry::OnParrySuccess(FGameplayEventData Payload)
{
	bParrySucceeded = true;

	UE_LOG(LogTemp, Warning, TEXT("[Parry] 성공 수신 | 후딜 면제 | Actor=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()));
}

void UGA_PlayerParry::OnReleaseReceived(FGameplayEventData Payload)
{
	bInputReleased = true;
}

void UGA_PlayerParry::OnWindowExpired()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	// R 을 아직 쥐고 있으면 방어태세로 넘긴다.
	//
	// 방어태세를 안 배웠으면 ActivationRequiredTags 에서 발동이 실패하고
	// 그대로 아래 후딜로 떨어진다. 여기서 증강 보유를 따로 검사하지 않는 이유다.
	if (!bInputReleased)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			if (ASC->TryActivateAbilityByClass(UGA_GuardStance::StaticClass()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Parry] 방어태세로 인계 | Actor=%s"),
					*GetNameSafe(GetAvatarActorFromActorInfo()));

				EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
				return;
			}
		}
	}

	// 헛쳤을 때만 굳는다. 막아냈으면 곧바로 다시 쓸 수 있다.
	if (!bParrySucceeded && ActorInfo && ActorInfo->IsNetAuthority())
	{
		HBEffects::ApplyTaggedDuration(
			ActorInfo->AbilitySystemComponent.Get(), RecoveryDuration, StateTag::TAG_State_ParryRecovery);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Parry] 창 닫힘 | Actor=%s Success=%d 후딜=%.2f"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), bParrySucceeded ? 1 : 0,
		bParrySucceeded ? 0.f : RecoveryDuration);

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
}
