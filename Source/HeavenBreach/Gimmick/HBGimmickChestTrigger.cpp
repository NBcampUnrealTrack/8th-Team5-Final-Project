#include "Gimmick/HBGimmickChestTrigger.h"

#include "AI/EnemyCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Loot/BoxActor.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "TimerManager.h"

AHBGimmickChestTrigger::AHBGimmickChestTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AHBGimmickChestTrigger::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	for (const AActor* Actor : GetLevel()->Actors)
	{
		const AHBChestSpawnPoint* Point = Cast<AHBChestSpawnPoint>(Actor);
		if (Point && Point->GroupId == ChestGroupId)
		{
			ChestPointLocations.Add(Point->GetActorLocation());
		}
	}
	if (ChestPointLocations.Num() > 0 && Spawns.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(PollTimer, this, &AHBGimmickChestTrigger::Poll, 0.5f, true, 1.f);
	}
}

void AHBGimmickChestTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PollTimer);
	Super::EndPlay(EndPlayReason);
}

void AHBGimmickChestTrigger::Poll()
{
	if (bTriggered)
	{
		GetWorldTimerManager().ClearTimer(PollTimer);
		return;
	}

	const float RadiusSq = FMath::Square(ChestMatchRadius);
	int32 Found = 0;
	int32 Opened = 0;
	for (const FVector& PointLocation : ChestPointLocations)
	{
		const ABoxActor* Best = nullptr;
		float BestDistSq = RadiusSq;
		for (const AActor* Actor : GetLevel()->Actors)
		{
			const ABoxActor* Box = Cast<ABoxActor>(Actor);
			if (!IsValid(Box))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(PointLocation, Box->GetActorLocation());
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Box;
			}
		}
		if (Best)
		{
			++Found;
			if (Best->GetBoxState() == EBoxState::Opened)
			{
				++Opened;
			}
		}
	}

	if (Found > 0 && Opened == Found)
	{
		Trigger();
	}
}

FTransform AHBGimmickChestTrigger::MakeSpawnTransform(const FHBGimmickSpawn& Spawn) const
{
	FVector Location = GetActorTransform().TransformPosition(Spawn.Location);
	const FRotator Rotation(0.f, GetActorRotation().Yaw + Spawn.Yaw, 0.f);

	float HalfHeight = 100.f;
	if (const AEnemyCharacterBase* Default = Spawn.EnemyClass ? Spawn.EnemyClass->GetDefaultObject<AEnemyCharacterBase>() : nullptr)
	{
		if (const UCapsuleComponent* Capsule = Default->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GimmickSpawnGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 300.f), Location - FVector(0, 0, 1000.f), ECC_Visibility, Query))
	{
		Location.Z = Hit.ImpactPoint.Z;
	}
	Location.Z += HalfHeight + 5.f;
	return FTransform(Rotation, Location);
}

void AHBGimmickChestTrigger::Trigger()
{
	bTriggered = true;
	GetWorldTimerManager().ClearTimer(PollTimer);

	TArray<AEnemyCharacterBase*> Spawned;
	for (const FHBGimmickSpawn& Spawn : Spawns)
	{
		if (!Spawn.EnemyClass)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.OverrideLevel = GetLevel();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AEnemyCharacterBase* Enemy = GetWorld()->SpawnActor<AEnemyCharacterBase>(Spawn.EnemyClass, MakeSpawnTransform(Spawn), Params);
		if (!Enemy)
		{
			continue;
		}
		if (!Enemy->GetController())
		{
			Enemy->SpawnDefaultController();
		}
		Enemy->ForceNetUpdate();
		Spawned.Add(Enemy);
	}
	BP_OnGimmickTriggered(Spawned);
}
