// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "EnemyAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UEnemyAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	// 초기 스탯 GE 적용. 서버 전용.
	//void ApplyInitStats(TSubclassOf<UGameplayEffect> InitEffect, float InMaxHP);
	void InitStats(float InMaxHP, float InAttackPower);

	void SetupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& InAbilities);

	// 해당 태그를 가진 어빌리티가 존재하나 확인
	// true : 활성화, false : 비활성화
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool IsAbilityActiveByTag(FGameplayTag AbilityTag) const;

	// 특정 어빌리티 태그를 보유했는지 확인
	// IsAbilityActiveByTag와 다른점은 해당 어빌리티 활성화까지는 안봄
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool HasAbilityByTag(FGameplayTag AbilityTag) const;
};
