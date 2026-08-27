// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AlertComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"

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

	// 1초마다 유효한 타겟(사망) 확인
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TargetValidTimer, this, &UAlertComponent::ValidTargets, 1.f, true);
	}

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

	LastKnownLocation = LastSeenLocation;

	UE_LOG(LogEnemyAI, Log, TEXT("[Sight] 시야 이탈 (기억 유지) -> %s"), *LastKnownLocation.ToString());
	OnAlertUpdated.Broadcast();
}

void UAlertComponent::SightForgotten(AActor* Actor)
{
	// 감지된 타겟에서 제거
	SensedTargets.Remove(TWeakObjectPtr<AActor>(Actor));
	PruneSensedTargets();

	// 다음타겟으로 변경
	if (AActor* NextTarget = PickTarget())
	{
		if (NextTarget != CurrentTarget.Get())
		{
			CurrentTarget = NextTarget;
		}

		// 교전상태 유지
		SetState(EAlertState::Engaged);
		OnAlertUpdated.Broadcast();
		return;
	}
	// 다음 타겟이 없음
	else
	{
		LastKnownLocation = Actor->GetActorLocation();
		CurrentTarget.Reset();
		UE_LOG(LogEnemyAI, Log, TEXT("[Sight] 놓침 -> 마지막 위치 %s"),*LastKnownLocation.ToCompactString());

		// 마지막 위치로 향하는 놓침상태
		SetState(EAlertState::Lost);
		OnAlertUpdated.Broadcast();

		if (UWorld* World = GetWorld())
		{
			// 시간이 지나면 idle로 돌아가도록 설정
			World->GetTimerManager().SetTimer(LostMemoryTimer, this, &UAlertComponent::OnLostMemoryExpired, LostMemoryDuration, false);
		}
	}
}

void UAlertComponent::DamageSensed(const FVector& InstigatorLocation)
{
	// 교전중에는 무시
	if (State == EAlertState::Engaged)
	{
		return;
	}

	// 해당 위치를 마지막으로 알고있는 위치로 기억하고 상태변경
	LastKnownLocation = InstigatorLocation;
	SetState(EAlertState::Lost);

	// 수색 시간 갱신
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LostMemoryTimer, this, &UAlertComponent::OnLostMemoryExpired, LostMemoryDuration, false);
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Damage] 피격 → 조사 지점 %s"), *InstigatorLocation.ToString());

	// 변경되었음을 알림
	OnAlertUpdated.Broadcast();
}

bool UAlertComponent::IsTargetAlive(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	// ASC가 없으면 그냥 살아있는걸로 친다.
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return true;
	}

	// 사망 태그 확인
	bool bIsDead = ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);

	return !bIsDead;
}

void UAlertComponent::SetState(EAlertState NewState)
{
	// 블랙보드에 반영 시 문제될수도잇으니 동일한 상태면 막음
	if (State == NewState)
		return;

	const EAlertState OldState = State;
	State = NewState;

	UE_LOG(LogEnemyAI, Log, TEXT("[Alert] %s: %s → %s"), *GetNameSafe(GetSensingActor()), LexToString(OldState), LexToString(NewState));
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

void UAlertComponent::ValidTargets()
{
	// 죽은 대상을 후보 목록에서 뺀다.
	// 뒤에서부터 도는 이유는 중간을 지워도 인덱스가 안 밀리기 때문이다.
	for (int32 i = SensedTargets.Num() - 1; i >= 0; --i)
	{
		if (!IsTargetAlive(SensedTargets[i].Get()))
		{
			SensedTargets.RemoveAt(i);
		}
	}

	AActor* Target = CurrentTarget.Get();

	// 타겟이 없으면 할 일이 없다.
	if (!Target)
	{
		return;
	}

	// 살아 있으면 그대로 둔다.
	if (IsTargetAlive(Target))
	{
		return;
	}
	else
	{
		// 마지막으로 기억한 자리를 시체의 위치를 기억한다.
		LastKnownLocation = Target->GetActorLocation();

		// 대상은 잊어버려야하는타겟으로 정리
		SightForgotten(Target);
	}
}
