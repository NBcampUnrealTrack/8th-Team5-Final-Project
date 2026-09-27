// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemySummonGABase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"

#include "AI/EnemyCharacterBase.h"
#include "AI/SummonComponent.h"
#include "AI/Enum/EAlertState.h"
#include "Core/Tag/Event/HB_EventTag.h"

bool UEnemySummonGABase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 쿨타임·차단 태그 같은 기본 조건이 먼저다.
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 꺼낼 것이 없으면 발동할 이유가 없다. 애셋 설정 누락도 여기서 걸린다.
	if (SummonEntries.Num() == 0)
	{
		return false;
	}

	// GetSummonComp 를 쓰지 않는다. 그쪽은 CurrentActorInfo 를 보는데,
	// 발동 판정 시점에는 아직 안 채워져 있을 수 있다. 넘어온 것을 그대로 쓴다.
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return false;
	}

	const USummonComponent* Comp = Avatar->FindComponentByClass<USummonComponent>();
	if (!Comp)
	{
		// 명부가 없으면 정원을 셀 수 없다. 무제한 소환이 되느니 막는다.
		return false;
	}

	return Comp->CountAlive() < MaxAlive;
}

void UEnemySummonGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모의 몽타주 실행/종료 기능 사용. CommitAbility 도 여기에 있음
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주가 안 나갔으면 이어서 할 게 없다
	if (!IsActive())
	{
		return;
	}

	PendingPoints.Reset();

	UAbilityTask_WaitGameplayEvent* WaitMark = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Summon_Mark, nullptr, false, false);
	WaitMark->EventReceived.AddDynamic(this, &UEnemySummonGABase::OnMarkEventReceived);
	WaitMark->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WaitSpawn = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Summon_Spawn, nullptr, false, false);
	WaitSpawn->EventReceived.AddDynamic(this, &UEnemySummonGABase::OnSpawnEventReceived);
	WaitSpawn->ReadyForActivation();

	UE_LOG(LogEnemyAI, Log, TEXT("[Summon] 소환 어빌리티 활성화"));
}

void UEnemySummonGABase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 몽타주가 중간에 끊기면 Spawn 노티파이가 안 온다.
	// 안 비우면 다음 소환 때 옛 지점이 섞여 나온다.
	PendingPoints.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemySummonGABase::OnMarkEventReceived(FGameplayEventData Payload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// 소환은 서버 전용
	if (!Avatar->HasAuthority())
	{
		return;
	}

	if (SummonEntries.Num() == 0)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Summon] SummonEntries 비어 있음: %s"), *GetName());
		return;
	}

	USummonComponent* Comp = GetSummonComp();
	if (!Comp)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Summon] USummonComponent 없음: %s"), *GetNameSafe(Avatar));
		return;
	}

	// 정원에서 남은 만큼만 부른다.
	// 정원이 찼으면 예고가 하나도 안 떠서 플레이어가 화면으로 안다.
	int32 Room = MaxAlive - Comp->GetAliveCount();
	if (Room <= 0)
	{
		UE_LOG(LogEnemyAI, Log, TEXT("[Summon] 정원 참 (%d/%d)"), Comp->GetAliveCount(), MaxAlive);
		return;
	}

	PendingPoints.Reset();

	const FVector Origin = Avatar->GetActorLocation();

	// 배열 순서대로 자리를 가져간다. 위에 있는 줄이 우선이다.
	for (int32 EntryIndex = 0; EntryIndex < SummonEntries.Num(); ++EntryIndex)
	{
		const FEnemySummonEntry& Entry = SummonEntries[EntryIndex];
		if (!Entry.SummonClass)
		{
			continue;
		}

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			if (Room <= 0)
			{
				break;
			}

			FVector Point = FVector::ZeroVector;
			if (!FindSpawnPoint(Origin, Point))
			{
				continue;
			}

			FSummonPoint Planned;
			Planned.EntryIndex = EntryIndex;
			Planned.Location = Point;
			PendingPoints.Add(Planned);

			PlayCueAt(MarkCueTag, Point);
			--Room;
		}
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Summon] 예고 %d개 (정원 %d/%d)"), PendingPoints.Num(), Comp->GetAliveCount(), MaxAlive);
}

void UEnemySummonGABase::OnSpawnEventReceived(FGameplayEventData Payload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	if (!Avatar->HasAuthority())
	{
		return;
	}

	USummonComponent* Comp = GetSummonComp();
	if (!Comp)
	{
		return;
	}

	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = Avatar;
	Params.Instigator = Cast<APawn>(Avatar);

	int32 Spawned = 0;
	for (const FSummonPoint& Planned : PendingPoints)
	{
		if (!SummonEntries.IsValidIndex(Planned.EntryIndex))
		{
			continue;
		}

		const TSubclassOf<AEnemyCharacterBase>& SpawnClass = SummonEntries[Planned.EntryIndex].SummonClass;
		if (!SpawnClass)
		{
			continue;
		}

		// 캡슐 절반만큼 띄운다. 네비 지점은 바닥이라 그대로 꺼내면 발이 묻힌다.
		float HalfHeight = 88.f;
		if (const ACharacter* CDO = SpawnClass->GetDefaultObject<ACharacter>())
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		const FVector SpawnLoc = Planned.Location + FVector(0.f, 0.f, HalfHeight);

		// 회전은 소환자와 같은 방향.
		// 타겟을 보게 할 수도 있지만 AI 가 곧바로 돌아서므로 값을 더 두지 않는다.
		AEnemyCharacterBase* Minion = World->SpawnActor<AEnemyCharacterBase>(SpawnClass, SpawnLoc, Avatar->GetActorRotation(), Params);
		if (!Minion)
		{
			continue;
		}

		// 드랍 없음 · 루팅 불가 · 죽으면 사라짐. 경험치는 그대로 준다
		Minion->MarkAsSummoned(DespawnDelay);
		Comp->Register(Minion);
		PlayCueAt(SpawnCueTag, Planned.Location);

		++Spawned;
	}

	PendingPoints.Reset();

	UE_LOG(LogEnemyAI, Log, TEXT("[Summon] %s 가 %d마리 소환"), *GetNameSafe(Avatar), Spawned);
}

bool UEnemySummonGABase::FindSpawnPoint(const FVector& Origin, FVector& OutPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	if (!Nav)
	{
		return false;
	}

	const float Angle = FMath::FRandRange(0.f, 360.f);
	const float Radius = FMath::FRandRange(SpawnRadiusMin, FMath::Max(SpawnRadiusMin, SpawnRadiusMax));

	const FVector Raw = Origin + FRotator(0.f, Angle, 0.f).Vector() * Radius;

	// 네비 위로 끌어올린다.
	// 이게 없으면 벽 안이나 허공에 적이 나온다.
	FNavLocation Projected;
	if (!Nav->ProjectPointToNavigation(Raw, Projected, NavProjectExtent))
	{
		return false;
	}

	OutPoint = Projected.Location;

	if (bDrawDebug)
	{
		DrawDebugSphere(World, OutPoint, 50.f, 12, FColor::Purple, false, 3.f);
	}

	return true;
}

void UEnemySummonGABase::PlayCueAt(const FGameplayTag& CueTag, const FVector& Location)
{
	if (!CueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = Location;
	CueParams.Instigator = GetAvatarActorFromActorInfo();

	// 서버에서 부르면 전 클라로 멀티캐스트된다
	ASC->ExecuteGameplayCue(CueTag, CueParams);
}

USummonComponent* UEnemySummonGABase::GetSummonComp() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	return Avatar->FindComponentByClass<USummonComponent>();
}
