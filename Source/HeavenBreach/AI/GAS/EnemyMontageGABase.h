// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnemyMontageGABase.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UEnemyMontageGABase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEnemyMontageGABase();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 角青矫懦 根鸥林
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;


	// 根鸥林啊 场巢
	UFUNCTION()
	void OnMontageFinished();

	// 根鸥林 吝窜
	UFUNCTION()
	void OnMontageCancelled();
};
