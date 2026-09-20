#include "Combat/Block/GA_GuardStance.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Animation/AnimMontage.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Player/HBPlayerCharacter.h"

UGA_GuardStance::UGA_GuardStance()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 유지되는 동안 이 태그를 든다. ApplyMeleeHit 이 이것을 보고 피해를 0 으로 만든다.
	// UpdateManaResource 도 같은 태그를 보고 자원을 태운다 — 구 방어와 같은 자리다.
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Blocking);

	// 방어태세를 배우지 않았으면 발동 자체가 막힌다.
	// 덕분에 패링 쪽에서 "증강을 가졌나" 를 따로 검사하지 않아도 된다.
	ActivationRequiredTags.AddTag(WarriorTag::TAG_Passive_GuardStance);

	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);
}

void UGA_GuardStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitGameplayEvent* WaitRelease = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EventTag::TAG_Event_Block_Release, nullptr, false, false);
	WaitRelease->EventReceived.AddDynamic(this, &UGA_GuardStance::OnReleaseReceived);
	WaitRelease->ReadyForActivation();

	// 방패를 든 자세를 유지한다. 패링과 같은 몽타주이고 캐릭터가 주인이다.
	//
	// 몽타주는 Start(드는 동작) → Loop(유지) 구조다. 섹션을 지정하지 않으면
	// 처음부터 재생되고 Loop 에서 자기 자신을 물어 R 을 뗄 때까지 유지된다.
	//
	// End 섹션(AN_Warrior_Block_Out, 내리는 동작)은 쓰지 않는다.
	//
	// 왜: 구 GA_PlayerBlock 은 방어 해제 때 MontageJumpToSection("End") 로
	// 내리는 동작을 다 보여주고 그것이 끝난 뒤에 EndAbility 를 불렀다. 그러면
	// 내리는 동안에도 State.Blocking 이 붙어 있는데, 구 방어는 70% 감소라
	// 그 구간이 크게 문제되지 않았다. 지금은 완전 무효다. 방패를 내리는
	// 0.3~0.5초 동안 무적이면 R 을 떼는 것으로 무적을 늘릴 수 있다.
	//
	// 그래서 R 을 떼는 즉시 어빌리티를 끝내고(무적·감속 해제), 내려오는 모습은
	// 태스크의 블렌드 아웃에 맡긴다. 게임 상태가 연출을 기다리지 않는다.
	//
	// 쳐낼 조건: 내리는 동작을 꼭 보여줘야 하면, EndAbility 로 태그를 먼저
	// 떼고 몽타주만 따로 End 섹션으로 보내는 방법이 있다. 어빌리티 수명과
	// 연출 수명을 분리하는 작업이라 그때 따로 설계한다.
	if (const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAnimMontage* Montage = Player->GetGuardMontage())
		{
			if (UAbilityTask_PlayMontageAndWait* MontageTask =
				UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f))
			{
				MontageTask->ReadyForActivation();
			}
		}
	}

	// 감속은 서버에서만 건다. 어트리뷰트 복제로 클라가 따라온다.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
				UGE_GuardStanceSlow::StaticClass(), 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Guard] 방어태세 시작 | Actor=%s Server=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), (ActorInfo && ActorInfo->IsNetAuthority()) ? 1 : 0);
}

void UGA_GuardStance::OnReleaseReceived(FGameplayEventData Payload)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_GuardStance::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveSlow(ActorInfo);

	UE_LOG(LogTemp, Warning, TEXT("[Guard] 방어태세 종료 | Actor=%s Cancelled=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), bWasCancelled ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_GuardStance::RemoveSlow(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!SlowHandle.IsValid())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(SlowHandle);
	}
	SlowHandle.Invalidate();
}
