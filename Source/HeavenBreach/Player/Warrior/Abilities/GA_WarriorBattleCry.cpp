#include "Player/Warrior/Abilities/GA_WarriorBattleCry.h"

#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"
#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"
#include "Player/HBPlayerCharacter.h"

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
		const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
		const int32 UpgradeLevel = Player
			? Player->GetSkillUpgradeLevel(TEXT("Warrior.BattleCry"))
			: 0;
		const float Duration = UpgradeLevel >= 1 ? 7.f : 5.f;
		const float AttackMultiplier = UpgradeLevel >= 2 ? 1.3f : 1.2f;
		const float DefenseBonus = UpgradeLevel >= 3 ? 20.f : 0.f;
		HBWarriorEffects::TryApplyCooldown(ASC, WarriorTag::TAG_Cooldown_BattleCry, CooldownDuration);
		HBWarriorEffects::ApplyBattleCryBuff(
			ASC, Duration, AttackMultiplier, 0.2f, DefenseBonus);
		// Future Lv.3 hook: find nearby teammates and apply a reduced version of this buff.
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
