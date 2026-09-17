#include "Combat/Block/GA_PlayerParry.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Combat/Effect/HBEffectLibrary.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
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
	ActivationBlockedTags.AddTag(StateTag::TAG_State_AfterDelay);

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

	// 창은 서버에서만 연다. 판정도 서버에서만 돌기 때문이다.
	if (ActorInfo->IsNetAuthority())
	{
		HBEffects::ApplyTaggedDuration(
			ActorInfo->AbilitySystemComponent.Get(), WindowDuration, StateTag::TAG_State_ParryWindow);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Parry] 창 열림 | Actor=%s Server=%d Duration=%.2f"),
		*GetNameSafe(Player), ActorInfo->IsNetAuthority() ? 1 : 0, WindowDuration);

	UAbilityTask_WaitDelay* WaitWindow = UAbilityTask_WaitDelay::WaitDelay(this, WindowDuration);
	WaitWindow->OnFinish.AddDynamic(this, &UGA_PlayerParry::OnWindowExpired);
	WaitWindow->ReadyForActivation();
}

void UGA_PlayerParry::OnWindowExpired()
{
	UE_LOG(LogTemp, Warning, TEXT("[Parry] 창 닫힘 | Actor=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()));

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
