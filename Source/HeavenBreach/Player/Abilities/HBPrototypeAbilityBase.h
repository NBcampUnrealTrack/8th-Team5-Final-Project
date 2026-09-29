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
	// 스킬 피해. 장비의 스킬 피해 옵션을 곱한다.
	// 주의: 기본 공격에는 쓰지 말 것. 스킬 배율이 기본 공격에도 붙는다.
	// 기본 공격은 GA_PlayerAttackRE 경로(근접 트레이스)를 탄다.
	bool ApplySkillDamage(AHBPlayerCharacter* Player, AActor* Target, float Damage) const;
	void AddMana(const FGameplayAbilityActorInfo* ActorInfo, float Amount) const;
};
