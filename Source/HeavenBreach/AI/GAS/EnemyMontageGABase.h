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
	// 실행시킬 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	// 몽타주와 함께 적용할 GE
	// 경직시간 추가하려고 만듬. 대상에게 GE부여는 몽타주 안 애님노티파이에서 써야한다. 이거아님
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UGameplayEffect> ActivationEffect;


	// 몽타주가 끝남
	UFUNCTION()
	void OnMontageFinished();

	// 몽타주 중단
	UFUNCTION()
	void OnMontageCancelled();
};
