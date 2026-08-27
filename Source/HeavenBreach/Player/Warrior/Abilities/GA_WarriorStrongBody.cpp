#include "Player/Warrior/Abilities/GA_WarriorStrongBody.h"

#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"
#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"

UGA_WarriorStrongBody::UGA_WarriorStrongBody()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_WarriorStrongBody::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PassiveEffectHandle = HBWarriorEffects::ApplyEffect(
		ASC, UGE_WarriorStrongBodyPassive::StaticClass());

	// 상시 패시브이므로 종료하지 않고 활성 상태를 유지합니다.
}

void UGA_WarriorStrongBody::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (PassiveEffectHandle.IsValid() && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffect(PassiveEffectHandle);
		}
		PassiveEffectHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
