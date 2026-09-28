// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerAttackRE.generated.h"

/**
 * 근접 기본 공격. 무기 종류마다 BP를 하나씩 만들어 쓴다.
 * 어느 무기용인지는 BP 의 AssetTags 에 Weapon.타입 을 넣어 정한다.
 * 판정은 몽타주의 ANS_MeleeHitDetect 가 담당하므로 여기서는 재생만 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerAttackRE : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_PlayerAttackRE();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageCancelled();

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AM_Attack;

	// 공격중 이속감소 GE
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> AttackSlowEffect;

	// 사용중인 이속감소 GE의 핸들. 취소시켜야하니 들고있는다.
	FActiveGameplayEffectHandle SlowHandle;

	// 발동 중에만 붙여 둔 타격 성질 큐 태그. 끝날 때 떼려고 들고 있는다.
	FGameplayTag AppliedImpactCueTag;

	void RemoveAttackSlow(const FGameplayAbilityActorInfo* ActorInfo);

	// 장착 무기에 맞는 큐 태그를 붙이고 뗀다. 붙이는 곳과 떼는 곳을 한 쌍으로 둔다.
	void ApplyImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo, class AHBPlayerCharacter* Player);
	void RemoveImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo);
};
