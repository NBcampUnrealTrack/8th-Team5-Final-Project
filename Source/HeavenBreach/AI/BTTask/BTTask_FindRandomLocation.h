// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindRandomLocation.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UBTTask_FindRandomLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_FindRandomLocation();
	
	// 작업 완료까지 진행되는 Task작업
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	virtual FString GetStaticDescription() const override;

protected:
	// 배회의 중심점
	UPROPERTY(EditAnywhere, Category = "Search")
	FBlackboardKeySelector OriginKey;

	// 이동되는 배회 거리
	UPROPERTY(EditAnywhere, Category = "Search", meta = (ClampMin = "0.0"))
	float Radius = 1000.f;
};
