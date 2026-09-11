#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

class UAbilitySystemComponent;
class UGameplayEffect;

namespace HBEffects
{
	FActiveGameplayEffectHandle ApplyEffect(
		UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass);

	FActiveGameplayEffectHandle ApplyTaggedDuration(
		UAbilitySystemComponent* ASC, float Duration, FGameplayTag GrantedTag);

	FActiveGameplayEffectHandle ApplyBattleCryBuff(
		UAbilitySystemComponent* ASC, float Duration, float AttackMultiplier,
		float MoveSpeedBonus, float DefenseBonus);

	bool TryApplyCooldown(UAbilitySystemComponent* ASC, FGameplayTag CooldownTag, float Duration);
	bool IsCooldownActive(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag);
	float GetCooldownRemaining(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag);
}
