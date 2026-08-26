#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HBPrototypeAbilityBase.generated.h"

class AHBPlayerCharacter;

/** Shared server-authoritative helpers for class skill prototypes without final VFX/projectiles. */
UCLASS(Abstract)
class HEAVENBREACH_API UHBPrototypeAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHBPrototypeAbilityBase();

protected:
	bool BeginAbilityWithCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FGameplayTag CooldownTag, float CooldownDuration, float ManaCost = 0.f);
	void FinishAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	AActor* FindLineTarget(AActor* Avatar, float Range, float Radius = 60.f) const;
	TArray<AActor*> FindLineTargets(AActor* Avatar, float Range, float Radius = 60.f) const;
	TArray<AActor*> FindTargetsInRadius(AActor* Avatar, FVector Center, float Radius) const;
	TArray<AActor*> FindTargetsInCone(AActor* Avatar, float Range, float HalfAngleDegrees) const;
	bool ApplySkillDamage(AHBPlayerCharacter* Player, AActor* Target, float Damage) const;
	void AddMana(const FGameplayAbilityActorInfo* ActorInfo, float Amount) const;
};
