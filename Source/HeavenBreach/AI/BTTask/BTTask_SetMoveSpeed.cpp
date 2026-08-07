// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask/BTTask_SetMoveSpeed.h"

#include "AIController.h"
#include "AI/EnemyCharacterBase.h"

UBTTask_SetMoveSpeed::UBTTask_SetMoveSpeed()
{
	NodeName = TEXT("Set Move Speed");
}

EBTNodeResult::Type UBTTask_SetMoveSpeed::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller)
		return EBTNodeResult::Failed;
	if (!Controller)
	{
		return EBTNodeResult::Failed;
	}

	AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(Controller->GetPawn());
	if (!Enemy)
	{
		return EBTNodeResult::Failed;
	}

	Enemy->SetMoveSpeed(bRun);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_SetMoveSpeed::GetStaticDescription() const
{
	return FString();
}
