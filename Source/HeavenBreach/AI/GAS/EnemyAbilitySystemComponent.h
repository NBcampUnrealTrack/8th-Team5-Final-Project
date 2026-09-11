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

	// 어빌리티가 사용가능한 상태인지 파악하고 가능한 어빌리티를 반환함.
	// 반환된 어빌리티목록은 OutList로 넘겨주고 종합 가중치도 OutTotalWeight로 넘겨준다.
	void GetUsableAbilities(FGameplayTag AbilityTag, float DistanceToTarget,TArray<FGameplayAbilitySpec>& OutList, float& OutTotalWeight);

	// 특정 어빌리티 태그를 보유했는지 확인
	// IsAbilityActiveByTag와 다른점은 해당 어빌리티 활성화까지는 안봄
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool HasAbilityByTag(FGameplayTag AbilityTag);

	// 특정 어빌리티 태그를 보유 했는지 + 사용가능한 상태인지 확인
	// HasAbilityByTag와 다른점은 사용가능상태까지 확인한다.
	UFUNCTION(BlueprintPure, Category = "GAS")
	bool HasUsableAbilityByTag(FGameplayTag AbilityTag, float DistanceToTarget);
};
