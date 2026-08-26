#pragma once

#include "CoreMinimal.h"
#include "Player/Abilities/HBPrototypeAbilityBase.h"
#include "GA_ClassBasicAttacks.generated.h"

/** Server-authoritative bow trace prototype. Replace the trace presentation with a projectile later. */
UCLASS()
class HEAVENBREACH_API UGA_ArcherBasicAttack : public UHBPrototypeAbilityBase
{
	GENERATED_BODY()
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

/** Server-authoritative magic projectile trace prototype with no mana cost. */
UCLASS()
class HEAVENBREACH_API UGA_MageBasicAttack : public UHBPrototypeAbilityBase
{
	GENERATED_BODY()
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
