#include "Player/Warrior/HBWarriorCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

AHBWarriorCharacter::AHBWarriorCharacter() = default;

void AHBWarriorCharacter::OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem)
{
	Super::OnAbilitySystemInitialized(AbilitySystem);

	if (!HasAuthority() || bWarriorInitialized || !AbilitySystem)
	{
		return;
	}

	bWarriorInitialized = true;
	ApplyDefaultAttributes(AbilitySystem);
	GrantStartupAbilities(AbilitySystem);
	InitializeWarriorLoadout();
}

void AHBWarriorCharacter::ApplyDefaultAttributes(UAbilitySystemComponent* AbilitySystem)
{
	if (!DefaultAttributesEffect)
	{
		return;
	}

	const FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		DefaultAttributesEffect,
		1.0f,
		EffectContext);

	if (EffectSpec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	}
}

void AHBWarriorCharacter::GrantStartupAbilities(UAbilitySystemComponent* AbilitySystem)
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
		}
	}
}
