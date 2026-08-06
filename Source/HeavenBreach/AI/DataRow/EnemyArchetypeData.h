// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/DataRow/EnemyStatRow.h"
#include "EnemyArchetypeData.generated.h"

class UBehaviorTree;

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UEnemyArchetypeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 숫자의 출처. 지정하면 FallbackStats 를 덮는다. 지금은 비워둔다.
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	FDataTableRowHandle StatRow;

	// 테이블 미지정 또는 행 조회 실패 시 쓰는 값. 애셋 단독으로도 동작한다.
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	FEnemyStatRow FallbackStats;

	//// 애셋 참조. 테이블에 담지 못한 데이터들
	//UPROPERTY(EditDefaultsOnly, Category = "Behavior")
	//TObjectPtr<UBehaviorTree> IdleTree;
	//
	//UPROPERTY(EditDefaultsOnly, Category = "Behavior")
	//TObjectPtr<UBehaviorTree> EngagedTree;

	// TODO: TSubclassOf<UGameplayEffect> InitStatsEffect;
	// TODO: TArray<TSubclassOf<UGameplayAbility>> Abilities;
	// TODO: UPROPERTY(Instanced) TArray<TObjectPtr<UPatternDefBase>> Patterns;

	// getter
	const FEnemyStatRow& GetStats() const;
};
