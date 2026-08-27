#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WarriorActives.generated.h"

UCLASS(Abstract)
class HEAVENBREACH_API UGA_WarriorTimedAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WarriorTimedAbility();

protected:
	bool IsOnCooldown(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag CooldownTag) const;
	void ApplyCooldownAndEnd(float Duration, FGameplayTag CooldownTag);
};

UCLASS()
class HEAVENBREACH_API UGA_WarriorChargeSlash : public UGA_WarriorTimedAbility
{
	GENERATED_BODY()
public:
	UGA_WarriorChargeSlash();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	UPROPERTY(EditDefaultsOnly, Category="Charge Slash") float DashStrength = 900.f;
	UPROPERTY(EditDefaultsOnly, Category="Charge Slash") float DamageMultiplier = 1.2f;
	UPROPERTY(EditDefaultsOnly, Category="Charge Slash") float Cooldown = 5.f;
};

UCLASS()
class HEAVENBREACH_API UGA_WarriorCombatRoll : public UGA_WarriorTimedAbility
{
	GENERATED_BODY()
public:
	UGA_WarriorCombatRoll();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	UPROPERTY(EditDefaultsOnly, Category="Combat Roll") float RollStrength = 750.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat Roll") float Cooldown = 3.f;
};

UCLASS()
class HEAVENBREACH_API UGA_WarriorExecute : public UGA_WarriorTimedAbility
{
	GENERATED_BODY()
public:
	UGA_WarriorExecute();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	UPROPERTY(EditDefaultsOnly, Category="Execute") float ExecuteHealthRatio = 0.3f;
	UPROPERTY(EditDefaultsOnly, Category="Execute") float DamageMultiplier = 2.5f;
	UPROPERTY(EditDefaultsOnly, Category="Execute") float Cooldown = 8.f;
};

UCLASS()
class HEAVENBREACH_API UGA_WarriorChainPull : public UGA_WarriorTimedAbility
{
	GENERATED_BODY()
public:
	UGA_WarriorChainPull();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	UPROPERTY(EditDefaultsOnly, Category="Chain Pull") float Range = 700.f;
	UPROPERTY(EditDefaultsOnly, Category="Chain Pull") float PullStrength = 1200.f;
	UPROPERTY(EditDefaultsOnly, Category="Chain Pull") float Cooldown = 10.f;
};

UCLASS()
class HEAVENBREACH_API UGA_WarriorBloodThirst : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_WarriorBloodThirst();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	UPROPERTY(EditDefaultsOnly, Category="Blood Thirst") float Duration = 5.f;
	UPROPERTY(EditDefaultsOnly, Category="Blood Thirst") float Cooldown = 15.f;
};
