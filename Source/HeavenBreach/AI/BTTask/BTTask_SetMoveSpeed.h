// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetMoveSpeed.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UBTTask_SetMoveSpeed : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_SetMoveSpeed();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;

protected:
	// true : 달리기 속도, false 걷는 속도
	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bRun = false;
};
