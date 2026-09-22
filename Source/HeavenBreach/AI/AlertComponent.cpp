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

	// 후보가 없을 때 수색할 자리는 이 액터가 있던 곳이다.
	// 액터가 이미 사라졌으면 마지막으로 본 자리를 그대로 쓴다.
	const FVector Fallback = Actor ? Actor->GetActorLocation() : LastKnownLocation;
	LoseCurrentTarget(Fallback);
}

void UAlertComponent::LoseCurrentTarget(const FVector& FallbackLocation)
{
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
	LastKnownLocation = FallbackLocation;
	CurrentTarget.Reset();
	UE_LOG(LogEnemyAI, Log, TEXT("[Sight] 놓침 -> 마지막 위치 %s"), *LastKnownLocation.ToCompactString());

	// 마지막 위치로 향하는 놓침상태
	SetState(EAlertState::Lost);
	OnAlertUpdated.Broadcast();

	if (UWorld* World = GetWorld())
	{
		// 시간이 지나면 idle로 돌아가도록 설정
		World->GetTimerManager().SetTimer(LostMemoryTimer, this, &UAlertComponent::OnLostMemoryExpired, LostMemoryDuration, false);
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

void UAlertComponent::RequestRetarget()
{
	PruneSensedTargets();

	// 후보가 하나뿐이면 바꿀 게 없다
	if (SensedTargets.Num() <= 1)
	{
		return;
	}

	// 현재 타겟을 목록 맨 뒤로 보낸다. 그러면 PickTarget 이 다음 후보를 집는다.
	AActor* Old = CurrentTarget.Get();
	if (Old)
	{
		const int32 Index = SensedTargets.IndexOfByKey(TWeakObjectPtr<AActor>(Old));
		if (Index != INDEX_NONE)
		{
			SensedTargets.RemoveAt(Index);
			SensedTargets.Add(Old);
		}
	}

	AActor* NewTarget = PickTarget();
	if (!NewTarget)
	{
		return;
	}

	// 후보가 여럿이어도 나머지가 전부 무효면 같은 놈이 다시 나온다
	if (NewTarget == Old)
	{
		return;
	}

	CurrentTarget = NewTarget;
	UE_LOG(LogEnemyAI, Log, TEXT("[Alert] 타겟 교체: %s → %s"), *GetNameSafe(Old), *GetNameSafe(NewTarget));

	OnAlertUpdated.Broadcast();
}

bool UAlertComponent::IsTargetAlive(const AActor* Actor) const
{
	// 파괴 예정이거나 이미 파괴된 액터도 여기서 걸러진다.
	// 플레이어가 죽으면 결과 맵으로 트래블하면서 폰이 이 상태를 거친다.
	if (!IsValid(Actor))
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
		AActor* Actor = Candidate.Get();
		if (!Actor)
		{
			continue;
		}

		// 목록 청소(ValidTargets)는 1초 주기라 그 사이에 죽거나 사라진 놈이 남아 있다.
		// 고르는 순간에 한 번 더 보면 그 창이 통째로 닫힌다.
		if (!IsTargetAlive(Actor))
		{
			continue;
		}

		return Actor;
	}
	return nullptr;
}

void UAlertComponent::PruneSensedTargets()
{
	for (auto It = SensedTargets.CreateIterator(); It; ++It)
	{
		// 유효성 검사. IsValid 로 봐야 파괴 예정인 액터까지 걸린다.
		if (!::IsValid(It->Get()))
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
	// 죽었거나 사라진 대상을 후보 목록에서 뺀다.
	// 뒤에서부터 도는 이유는 중간을 지워도 인덱스가 안 밀리기 때문이다.
	//
	// IsTargetAlive 하나로 통일한 이유: 안에서 IsValid(존재) → 사망 태그 순으로 보므로
	// "없어졌다" 와 "죽었다" 를 따로 쓸 필요가 없다. 판단 기준을 한 곳에만 둔다.
	for (int32 i = SensedTargets.Num() - 1; i >= 0; --i)
	{
		if (!IsTargetAlive(SensedTargets[i].Get()))
		{
			SensedTargets.RemoveAt(i);
		}
	}

	AActor* Target = CurrentTarget.Get();

	// 타겟 액터가 통째로 사라진 경우다. 플레이어가 죽으면 결과 맵으로 트래블하면서
	// 폰이 파괴되고, 접속을 끊어도 같다. 여기서 그냥 나가면 상태가 Engaged 에 굳어
	// 적이 허공을 보고 선 채로 공격도 수색도 하지 않는다.
	if (!Target)
	{
		// Lost 면 이미 정리된 상태다. 여기서 또 부르면 수색 타이머가 매초 갱신되어
		// 영원히 Idle 로 못 내려간다.
		if (State == EAlertState::Engaged)
		{
			// 시체 위치를 못 구하니 마지막으로 본 자리를 그대로 쓴다.
			LoseCurrentTarget(LastKnownLocation);
		}
		return;
	}

	// 살아 있으면 그대로 둔다.
	if (IsTargetAlive(Target))
	{
		return;
	}

	// 대상은 잊어버려야하는타겟으로 정리. 시체 자리를 수색 지점으로 기억한다.
	SightForgotten(Target);
}
