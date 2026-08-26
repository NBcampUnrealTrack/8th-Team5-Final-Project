#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WarriorPassives.generated.h"

UCLASS(Abstract)
class HEAVENBREACH_API UGA_WarriorPassiveBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WarriorPassiveBase();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

UCLASS() class HEAVENBREACH_API UGA_WarriorCombatInstinct : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorCombatInstinct(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorBerserkerRage : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorBerserkerRage(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorConsecutiveAttack : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorConsecutiveAttack(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorKillingMomentum : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorKillingMomentum(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorWeaknessDetection : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorWeaknessDetection(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorCombatRecovery : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorCombatRecovery(); };
UCLASS() class HEAVENBREACH_API UGA_WarriorRevenge : public UGA_WarriorPassiveBase
{ GENERATED_BODY() public: UGA_WarriorRevenge(); };
