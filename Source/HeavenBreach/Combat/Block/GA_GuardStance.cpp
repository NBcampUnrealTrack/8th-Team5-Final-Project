#include "Combat/Block/GA_GuardStance.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

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
