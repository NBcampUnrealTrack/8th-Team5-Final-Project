#include "Player/Warrior/Abilities/GA_WarriorBattleCry.h"

#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"
#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"

UGA_WarriorBattleCry::UGA_WarriorBattleCry()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_WarriorBattleCry::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || HBWarriorEffects::IsCooldownActive(ASC, WarriorTag::TAG_Cooldown_BattleCry))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		HBWarriorEffects::TryApplyCooldown(ASC, WarriorTag::TAG_Cooldown_BattleCry, CooldownDuration);
		HBWarriorEffects::ApplyEffect(ASC, UGE_WarriorBattleCryBuff::StaticClass());
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
