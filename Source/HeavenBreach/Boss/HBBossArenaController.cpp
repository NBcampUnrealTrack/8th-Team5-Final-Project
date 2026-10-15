#include "Boss/HBBossArenaController.h"

#include "AI/EnemyCharacterBase.h"
#include "Boss/HBBossDoor.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Escape/ExtractionZone.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "TimerManager.h"

AHBBossArenaController::AHBBossArenaController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	ArenaTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaTrigger"));
	RootComponent = ArenaTrigger;
	ArenaTrigger->SetBoxExtent(FVector(1500.f, 1500.f, 600.f));
	ArenaTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArenaTrigger->SetCollisionObjectType(ECC_WorldStatic);
	ArenaTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArenaTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ArenaTrigger->SetGenerateOverlapEvents(true);
	ArenaTrigger->ShapeColor = FColor(255, 80, 40);
}

void AHBBossArenaController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHBBossArenaController, ArenaState);
}

void AHBBossArenaController::BeginPlay()
{
	Super::BeginPlay();
	ApplyExitVisibility();

	if (!HasAuthority())
	{
		return;
	}

	for (const AActor* Actor : GetLevel()->Actors)
	{
		const AHBEnemySpawnPoint* Point = Cast<AHBEnemySpawnPoint>(Actor);
		if (Point && Point->bAllowBoss)
		{
			BossAnchor = Point->GetActorLocation();
			bHasBossAnchor = true;
			break;
		}
	}

	SetDoors(true, false, true);
	ArenaTrigger->OnComponentBeginOverlap.AddDynamic(this, &AHBBossArenaController::OnTriggerBeginOverlap);
	GetWorldTimerManager().SetTimer(FindBossTimer, this, &AHBBossArenaController::FindBoss, 0.5f, true, 0.5f);
}

void AHBBossArenaController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LockTimer);
	GetWorldTimerManager().ClearTimer(FindBossTimer);
	if (AEnemyCharacterBase* Enemy = Boss.Get())
	{
		Enemy->OnDied.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AHBBossArenaController::FindBoss()
{
	if (Boss.IsValid() || ArenaState == EHBBossArenaState::Cleared)
	{
		GetWorldTimerManager().ClearTimer(FindBossTimer);
		return;
	}

	const FVector Origin = bHasBossAnchor ? BossAnchor : GetActorLocation();
	AEnemyCharacterBase* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(BossSearchRadius));
	for (AActor* Actor : GetLevel()->Actors)
	{
		AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(Actor);
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}
		if (BossClass && !Enemy->IsA(BossClass))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Origin, Enemy->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}

	if (Best)
	{
		Boss = Best;
		Best->OnDied.AddUObject(this, &AHBBossArenaController::HandleBossDied);
		GetWorldTimerManager().ClearTimer(FindBossTimer);
	}
}

bool AHBBossArenaController::IsBossAlive() const
{
	const AEnemyCharacterBase* Enemy = Boss.Get();
	return Enemy && !Enemy->IsDead();
}

void AHBBossArenaController::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || !Pawn || !Pawn->IsPlayerControlled() || ArenaState != EHBBossArenaState::Waiting)
	{
		return;
	}
	if (!IsBossAlive())
	{
		FindBoss();
		if (!IsBossAlive())
		{
			return;
		}
	}

	SetArenaState(EHBBossArenaState::Fighting);
	SetDoors(false, true, false);
	if (LockSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(LockTimer, this, &AHBBossArenaController::OnLockExpired, LockSeconds, false);
	}
}

void AHBBossArenaController::OnLockExpired()
{
	if (ArenaState != EHBBossArenaState::Fighting)
	{
		return;
	}
	SetDoors(true, false, false);
	SetArenaState(EHBBossArenaState::Waiting);
}

void AHBBossArenaController::HandleBossDied(AEnemyCharacterBase* Enemy)
{
	if (!HasAuthority() || Enemy != Boss.Get())
	{
		return;
	}
	Enemy->OnDied.RemoveAll(this);
	GetWorldTimerManager().ClearTimer(LockTimer);
	SetDoors(true, false, false);
	SetArenaState(EHBBossArenaState::Cleared);
}

void AHBBossArenaController::SetDoors(bool bOpen, bool bLock, bool bInstant)
{
	for (AHBBossDoor* Door : ArenaDoors)
	{
		if (IsValid(Door))
		{
			Door->SetDoorState(bOpen, bLock, bInstant);
		}
	}
}

void AHBBossArenaController::SetArenaState(EHBBossArenaState NewState)
{
	if (ArenaState == NewState)
	{
		return;
	}
	ArenaState = NewState;
	OnRep_ArenaState();
}

void AHBBossArenaController::OnRep_ArenaState()
{
	ApplyExitVisibility();
	BP_OnArenaStateChanged(ArenaState);
}

void AHBBossArenaController::ApplyExitVisibility()
{
	if (IsValid(ExitZone))
	{
		ExitZone->SetExtractionActive(ArenaState == EHBBossArenaState::Cleared);
	}
}
