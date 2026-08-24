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
	void InitStats(float InMaxHP, float InAttackPower, float InAttackSpeedRate);

	void SetupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& InAbilities);

	// 기존 TryActivateAbilityByTag의 가중치 추가판.
	// 가중치에 따라 같은 어빌중에서 골라 랜덤으로 선택해 발동시킨다.
	// 반환 true :정상적, false: 실패
	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool TryActivateWeightedAbilityByTag(FGameplayTag AbilityTag, float DistanceToTarget);

	// 해당 태그를 가진 어빌리티가 존재하나 확인
	// true : 활성화, false : 비활성화
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool IsAbilityActiveByTag(FGameplayTag AbilityTag);

	// 특정 어빌리티 태그를 보유했는지 확인
	// IsAbilityActiveByTag와 다른점은 해당 어빌리티 활성화까지는 안봄
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool HasAbilityByTag(FGameplayTag AbilityTag);
};
