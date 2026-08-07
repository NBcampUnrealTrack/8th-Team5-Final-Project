// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask/BTTask_FindRandomLocation.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "NavigationSystem.h"

UBTTask_FindRandomLocation::UBTTask_FindRandomLocation()
{
	NodeName = TEXT("Find Random Location");

	// 결과 키와 중심점 키를 Vector 로 제한한다.
	// 안 하면 에디터에서 Object 키를 고를 수 있고, 런타임에 조용히 실패한다.
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomLocation, BlackboardKey));
	OriginKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomLocation, OriginKey));
	RadiusKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomLocation, RadiusKey));
}

EBTNodeResult::Type UBTTask_FindRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
	if (!NavSys)
	{
		// 레벨에 NavMeshBoundsVolume 이 없다
		return EBTNodeResult::Failed;
	}

	const FVector Origin = BB->GetValueAsVector(OriginKey.SelectedKeyName);
	const float Radius = BB->GetValueAsFloat(RadiusKey.SelectedKeyName);

	if (Radius <= 0.f)
	{
		return EBTNodeResult::Failed;
	}

	// GetRandomReachablePointInRadius: 네비메시 위 + 실제로 도달 가능한 지점만 반환.
	// GetRandomPointInNavigableRadius 는 네비메시 위만 보장하므로
	// 벽 너머 같은 도달 불가 지점이 나올 수 있다.
	FNavLocation RandomPoint;
	if (!NavSys->GetRandomReachablePointInRadius(Origin, Radius, RandomPoint))
	{
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsVector(BlackboardKey.SelectedKeyName, RandomPoint.Location);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_FindRandomLocation::GetStaticDescription() const
{
	// 에디터 노드에 요약을 표시한다. 트리가 커지면 이게 없으면 노드를 클릭해야 설정이 보인다.
	return FString::Printf(TEXT("%s 주변 %s 반경 -> %s"),
		*OriginKey.SelectedKeyName.ToString(),
		*RadiusKey.SelectedKeyName.ToString(),
		*BlackboardKey.SelectedKeyName.ToString());
}