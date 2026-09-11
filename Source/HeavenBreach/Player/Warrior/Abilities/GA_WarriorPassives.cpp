#include "Player/Warrior/Abilities/GA_WarriorPassives.h"

#include "Core/Tag/Warrior/HB_WarriorTag.h"

UGA_WarriorPassiveBase::UGA_WarriorPassiveBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_WarriorPassiveBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

UGA_WarriorCombatInstinct::UGA_WarriorCombatInstinct()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_CombatInstinct); }
UGA_WarriorBerserkerRage::UGA_WarriorBerserkerRage()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_BerserkerRage); }
UGA_WarriorConsecutiveAttack::UGA_WarriorConsecutiveAttack()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_ConsecutiveAttack); }
UGA_WarriorKillingMomentum::UGA_WarriorKillingMomentum()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_KillingMomentum); }
UGA_WarriorWeaknessDetection::UGA_WarriorWeaknessDetection()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_WeaknessDetection); }
UGA_WarriorCombatRecovery::UGA_WarriorCombatRecovery()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_CombatRecovery); }
UGA_WarriorRevenge::UGA_WarriorRevenge()
{ ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_Revenge); }
