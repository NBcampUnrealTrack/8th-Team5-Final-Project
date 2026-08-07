// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AlertComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UAlertComponent::UAlertComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UAlertComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void UAlertComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAlertComponent::SightSensed(AActor* Actor)
{
	if (!Actor)
		return;

	TWeakObjectPtr<AActor>sightActor(Actor);
	// 이미 존재하나 확인하고 추가
	if (!SensedTargets.Contains(Actor))
	{
		SensedTargets.Add(sightActor);
	}

	PruneSensedTargets();

	// 새 타겟 지정
	// 지금은 맨처음 본놈을 대상으로가지만 혹시모르니까...
	AActor* NewTarget = PickTarget();
	CurrentTarget = NewTarget;
	UE_LOG(LogEnemyAI, Log, TEXT("[Sight] 보임 → 타겟: %s"), *GetNameSafe(NewTarget));

	// 잊어버리기 타이머 취소.
	UWorld* World = GetWorld();
	if (World && World->GetTimerManager().IsTimerActive(LostMemoryTimer))
	{
		World->GetTimerManager().ClearTimer(LostMemoryTimer);
	}

	//시야에 타겟이있다? 교전상태로
	SetState(EAlertState::Engaged);

	OnAlertUpdated.Broadcast();
}

void UAlertComponent::SightLost(AActor* Actor, const FVector& LastSeenLocation)
{
	if (!Actor)
		return;

	SensedTargets.Remove(TWeakObjectPtr<AActor>(Actor));
	PruneSensedTargets();

	// 다음타겟으로 변경
	if (AActor* NextTarget = PickTarget())
	{
		if (NextTarget != CurrentTarget.Get())
		{
			CurrentTarget = NextTarget;
		}
		SetState(EAlertState::Engaged);
		OnAlertUpdated.Broadcast();
		return;
	}
	// 다음 타겟이 없음
	else
	{
		LastKnownLocation = LastSeenLocation;
		CurrentTarget.Reset();
		UE_LOG(LogEnemyAI, Log, TEXT("[Sight] 놓침 → 마지막 위치 %s"),
			*LastSeenLocation.ToCompactString());
		SetState(EAlertState::Lost);
		OnAlertUpdated.Broadcast();

		if (UWorld* World = GetWorld())
		{
			// 시간이 지나면 idle로 돌아가도록 설정
			World->GetTimerManager().SetTimer(LostMemoryTimer, this,
				&UAlertComponent::OnLostMemoryExpired, LostMemoryDuration, false);
		}
	}

}

void UAlertComponent::SetState(EAlertState NewState)
{
	// 블랙보드에 반영 시 문제될수도잇으니 동일한 상태면 막음
	if (State == NewState)
		return;

	const EAlertState OldState = State;
	State = NewState;

	UE_LOG(LogEnemyAI, Log, TEXT("[Alert] %s: %s → %s"),
		*GetNameSafe(GetSensingActor()), LexToString(OldState), LexToString(NewState));
}

AActor* UAlertComponent::PickTarget() const
{
	// TODO : 언젠가 타겟정하는바식이 바뀔수도 있음.
	// 현재 우선순위 = 맨처음 발견된 플레이어
	for (const TWeakObjectPtr<AActor>& Candidate : SensedTargets)
	{
		if (AActor* Actor = Candidate.Get())
		{
			return Actor;
		}
	}
	return nullptr;
}

void UAlertComponent::PruneSensedTargets()
{
	for (auto It = SensedTargets.CreateIterator(); It; ++It)
	{
		// 유효성 검사
		if (!It->IsValid())
		{
			// 해당 노드를 제거
			It.RemoveCurrent();
		}

	}
}

AActor* UAlertComponent::GetSensingActor() const
{
	// ai컨트롤러 찾아 빙의중인 폰 탐색
	if (const AController* AsController = Cast<AController>(GetOwner()))
	{
		return AsController->GetPawn();
	}
	// 원래 해당 컴포넌트는 ai컨트롤러용이지만 예외적 호영할 경우가 잇을수 있으니 폰이 주인일 경우도 허용
	return GetOwner();
}

void UAlertComponent::OnLostMemoryExpired()
{
	// 타이머가 도는 사이 상태가 바뀌었을 수 있다.
	if (State != EAlertState::Lost)
	{
		return;
	}

	// 정보초기화 하고 기본상태로 전환
	SensedTargets.Reset();
	CurrentTarget.Reset();
	SetState(EAlertState::Idle);
	OnAlertUpdated.Broadcast();
}
