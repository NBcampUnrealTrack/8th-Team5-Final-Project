#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

class UAbilitySystemComponent;
class UGameplayEffect;

namespace HBWarriorEffects
{
	FActiveGameplayEffectHandle ApplyEffect(
		UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass);

	FActiveGameplayEffectHandle ApplyTaggedDuration(
		UAbilitySystemComponent* ASC, float Duration, FGameplayTag GrantedTag);

	bool TryApplyCooldown(UAbilitySystemComponent* ASC, FGameplayTag CooldownTag, float Duration);
	bool IsCooldownActive(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag);
	float GetCooldownRemaining(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag);
}
