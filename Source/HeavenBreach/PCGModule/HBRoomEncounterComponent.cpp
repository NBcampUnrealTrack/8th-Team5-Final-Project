#include "PCGModule/HBRoomEncounterComponent.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "PCGModule/HBDungeonPortal.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationSystemTypes.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBRoomEncounter, Log, All);

namespace
{
	bool AllowsGrade(const AHBEnemySpawnPoint& Point, EHBEnemySpawnGrade Grade)
	{
		switch (Grade)
		{
		case EHBEnemySpawnGrade::Common: return Point.bAllowCommon;
		case EHBEnemySpawnGrade::Elite: return Point.bAllowElite;
		case EHBEnemySpawnGrade::Boss: return Point.bAllowBoss;
		}
		return false;
	}

	bool AllowsClass(const AHBEnemySpawnPoint& Point, UClass* Class)
	{
		return Point.AllowedClasses.IsEmpty() || Point.AllowedClasses.ContainsByPredicate(
			[Class](const TSubclassOf<AEnemyCharacterBase>& Allowed) { return Allowed.Get() == Class; });
	}

	int32 MakeRoomSeed(int32 DungeonSeed, int32 RoomId)
	{
		return static_cast<int32>(HashCombineFast(HashCombineFast(
			static_cast<uint32>(DungeonSeed), static_cast<uint32>(RoomId)), 0x4842454Eu));
	}

	void ShufflePoints(TArray<AHBEnemySpawnPoint*>& Points, FRandomStream& Random)
	{
		for (int32 Index = Points.Num() - 1; Index > 0; --Index)
		{
			Points.Swap(Index, Random.RandRange(0, Index));
		}
	}

	bool TryOptionsAtPoint(const AHBEnemySpawnPoint& Point, EHBEnemySpawnGrade Grade,
		const TArray<FHBEnemySpawnOption>& Pool, FRandomStream& Random,
		TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
		FHBEncounterSpawnChoice& OutChoice, FString& LastFailure)
	{
		TArray<FHBEnemySpawnOption> Remaining;
		for (const FHBEnemySpawnOption& Option : Pool)
		{
			if (Option.EnemyClass && Option.Weight > 0 && AllowsClass(Point, Option.EnemyClass.Get()))
			{
				Remaining.Add(Option);
			}
		}
		while (!Remaining.IsEmpty())
		{
			int32 TotalWeight = 0;
			for (const FHBEnemySpawnOption& Option : Remaining) TotalWeight += Option.Weight;
			int32 Roll = Random.RandRange(1, TotalWeight);
			int32 Pick = 0;
			for (; Pick < Remaining.Num() - 1; ++Pick)
			{
				Roll -= Remaining[Pick].Weight;
				if (Roll <= 0) break;
			}
			FHBEncounterSpawnChoice Choice{Point.PointId, const_cast<AHBEnemySpawnPoint*>(&Point), Remaining[Pick].EnemyClass, Grade};
			Remaining.RemoveAt(Pick);
			FString Reason;
			if (TryPlace(Choice, Reason))
			{
				OutChoice = Choice;
				return true;
			}
			LastFailure = FString::Printf(TEXT("Point %s: %s"), *Point.PointId.ToString(), *Reason);
		}
		return false;
	}
}

bool HBRoomEncounter::SelectAndPlace(const UHBRoomEncounterProfile& Profile,
	const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
	TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError)
{
	OutPlaced.Reset();
	OutError.Reset();
	TArray<FString> ProfileErrors;
	if (!Profile.Validate(ProfileErrors))
	{
		OutError = FString::Join(ProfileErrors, TEXT("; "));
		return false;
	}
	TArray<AHBEnemySpawnPoint*> SortedPoints = Points;
	SortedPoints.RemoveAll([](const AHBEnemySpawnPoint* Point) { return !IsValid(Point); });
	SortedPoints.Sort([](const AHBEnemySpawnPoint& A, const AHBEnemySpawnPoint& B)
	{
		return A.PointId.ToString() < B.PointId.ToString();
	});
	FRandomStream Random(MakeRoomSeed(DungeonSeed, RoomId));
	TSet<FName> UsedIds;
	FString LastFailure;
	if (Profile.BossClass)
	{
		AHBEnemySpawnPoint* const* Found = SortedPoints.FindByPredicate([&](const AHBEnemySpawnPoint* Point)
		{
			return Point->PointId == Profile.BossPointId && Point->bAllowBoss;
		});
		if (!Found || !AllowsClass(**Found, Profile.BossClass.Get()))
		{
			OutError = FString::Printf(TEXT("Room %d has no usable boss point %s."), RoomId, *Profile.BossPointId.ToString());
			return false;
		}
		FHBEncounterSpawnChoice Choice{(*Found)->PointId, *Found, Profile.BossClass, EHBEnemySpawnGrade::Boss};
		if (!TryPlace(Choice, LastFailure))
		{
			OutError = FString::Printf(TEXT("Room %d boss placement failed at %s: %s"), RoomId, *Choice.PointId.ToString(), *LastFailure);
			return false;
		}
		OutPlaced.Add(Choice);
		UsedIds.Add(Choice.PointId);
	}

	auto FillGrade = [&](EHBEnemySpawnGrade Grade, int32 Minimum, int32 Maximum,
		const TArray<FHBEnemySpawnOption>& Pool) -> bool
	{
		const int32 Target = Random.RandRange(Minimum, Maximum);
		TArray<AHBEnemySpawnPoint*> Required;
		TArray<AHBEnemySpawnPoint*> Optional;
		for (AHBEnemySpawnPoint* Point : SortedPoints)
		{
			if (UsedIds.Contains(Point->PointId) || !AllowsGrade(*Point, Grade)) continue;
			(Point->bRequired ? Required : Optional).Add(Point);
		}
		ShufflePoints(Required, Random);
		ShufflePoints(Optional, Random);
		Required.Append(Optional);
		int32 Placed = 0;
		for (AHBEnemySpawnPoint* Point : Required)
		{
			if (Placed >= Target) break;
			FHBEncounterSpawnChoice Choice;
			if (TryOptionsAtPoint(*Point, Grade, Pool, Random, TryPlace, Choice, LastFailure))
			{
				OutPlaced.Add(Choice);
				UsedIds.Add(Choice.PointId);
				++Placed;
			}
		}
		if (Placed < Minimum)
		{
			OutError = FString::Printf(TEXT("Room %d %s: placed %d of required %d. %s"), RoomId,
				Grade == EHBEnemySpawnGrade::Elite ? TEXT("elite") : TEXT("common"), Placed, Minimum, *LastFailure);
			return false;
		}
		if (Placed < Target)
		{
			UE_LOG(LogHBRoomEncounter, Warning, TEXT("Room %d %s: placed %d of selected %d (minimum %d). Last rejection: %s"),
				RoomId, Grade == EHBEnemySpawnGrade::Elite ? TEXT("elite") : TEXT("common"),
				Placed, Target, Minimum, *LastFailure);
		}
		return true;
	};
	if (!FillGrade(EHBEnemySpawnGrade::Elite, Profile.MinEliteCount, Profile.MaxEliteCount, Profile.EliteEnemies)
		|| !FillGrade(EHBEnemySpawnGrade::Common, Profile.MinCommonCount, Profile.MaxCommonCount, Profile.CommonEnemies))
	{
		return false;
	}
	for (const AHBEnemySpawnPoint* Point : SortedPoints)
	{
		if (Point->bRequired && !UsedIds.Contains(Point->PointId))
		{
			OutError = FString::Printf(TEXT("Room %d required point %s could not be filled. %s"), RoomId, *Point->PointId.ToString(), *LastFailure);
			return false;
		}
	}
	return true;
}

UHBRoomEncounterComponent::UHBRoomEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHBRoomEncounterComponent::IsPreparedFor(int32 GenerationId, int32 RoomId) const
{
	return bPrepared && ActiveGenerationId == GenerationId && ActiveRoomId == RoomId;
}

bool UHBRoomEncounterComponent::StartEncounter(int32 GenerationId, int32 DungeonSeed, int32 RoomId,
	const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError)
{
	OutError.Reset();
	if (IsPreparedFor(GenerationId, RoomId)) return true;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("Room encounters can only be spawned by the server.");
		return false;
	}
	if (bPrepared || !SpawnedEnemies.IsEmpty())
	{
		OutError = TEXT("Encounter already belongs to another generation; clear it before preparing again.");
		return false;
	}
	const auto* Definition = Cast<AHBDungeonModuleDefinition>(GetOwner());
	if (!Definition || !Definition->EncounterProfile || !RoomBounds.IsValid || EntryLocations.IsEmpty())
	{
		OutError = TEXT("Room encounter needs a profile, valid room bounds and at least one entry location.");
		return false;
	}
	TArray<AHBEnemySpawnPoint*> Points;
	for (AActor* Actor : Definition->GetLevel()->Actors)
	{
		if (auto* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point)) Points.Add(Point);
	}
	TArray<FHBEncounterSpawnChoice> Placed;
	const bool bSucceeded = HBRoomEncounter::SelectAndPlace(*Definition->EncounterProfile, Points,
		DungeonSeed, RoomId,
		[&](const FHBEncounterSpawnChoice& Choice, FString& Reason)
		{
			AEnemyCharacterBase* Enemy = nullptr;
			if (!TrySpawn(Choice, RoomBounds, EntryLocations, Enemy, Reason)) return false;
			SpawnedEnemies.Add(Enemy);
			Enemy->OnDied.AddUObject(this, &UHBRoomEncounterComponent::HandleEnemyDied);
			++AliveCount;
			UE_LOG(LogHBRoomEncounter, Display, TEXT("Room %d point %s spawned %s at %s."),
				RoomId, *Choice.PointId.ToString(), *GetNameSafe(Choice.EnemyClass.Get()),
				*Enemy->GetActorLocation().ToString());
			return true;
		}, Placed, OutError);
	if (!bSucceeded)
	{
		ClearEncounter();
		return false;
	}
	ActiveGenerationId = GenerationId;
	ActiveRoomId = RoomId;
	bPrepared = true;
	UE_LOG(LogHBRoomEncounter, Display, TEXT("Room %d encounter ready: %d enemies, seed %d."), RoomId, SpawnedEnemies.Num(), DungeonSeed);
	return true;
}

bool UHBRoomEncounterComponent::TrySpawn(const FHBEncounterSpawnChoice& Choice, const FBox& RoomBounds,
	const TArray<FVector>& EntryLocations, AEnemyCharacterBase*& OutEnemy, FString& OutReason)
{
	OutEnemy = nullptr;
	UWorld* World = GetWorld();
	const auto* Defaults = Choice.EnemyClass ? Choice.EnemyClass->GetDefaultObject<AEnemyCharacterBase>() : nullptr;
	const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	if (!World || !Choice.Point || !Capsule)
	{
		OutReason = TEXT("Missing world, point or enemy capsule.");
		return false;
	}
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if ((Choice.Point->MaxCapsuleRadius > 0 && Radius > Choice.Point->MaxCapsuleRadius)
		|| (Choice.Point->MaxCapsuleHalfHeight > 0 && HalfHeight > Choice.Point->MaxCapsuleHalfHeight))
	{
		OutReason = TEXT("Enemy capsule exceeds the point's size limit.");
		return false;
	}
	const FVector Marker = Choice.Point->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBEnemyFloor), false);
	Query.AddIgnoredActor(Choice.Point);
	FHitResult Floor;
	if (!World->LineTraceSingleByChannel(Floor, Marker + FVector(0, 0, 100), Marker - FVector(0, 0, 200), ECC_WorldStatic, Query)
		|| Floor.ImpactNormal.Z < MinFloorUpDot)
	{
		OutReason = TEXT("No safe floor or floor slope is too steep.");
		return false;
	}
	const FVector Ground = Floor.ImpactPoint;
	const FVector SpawnLocation = Ground + FVector(0, 0, HalfHeight + 2.f);
	if (!RoomBounds.IsInsideOrOn(SpawnLocation))
	{
		OutReason = TEXT("Projected spawn is outside the room bounds.");
		return false;
	}
	for (const FVector& Entry : EntryLocations)
	{
		if (FVector::Dist2D(Ground, Entry) < MinEntryDistance + Radius)
		{
			OutReason = TEXT("Spawn blocks a room entry or player start.");
			return false;
		}
	}
	for (AEnemyCharacterBase* Other : SpawnedEnemies)
	{
		if (IsValid(Other))
		{
			const float OtherRadius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
			if (FVector::Dist2D(SpawnLocation, Other->GetActorLocation()) < Radius + OtherRadius + MinMonsterSpacing)
			{
				OutReason = TEXT("Spawn is too close to another monster.");
				return false;
			}
		}
	}
	for (AActor* Actor : GetOwner()->GetLevel()->Actors)
	{
		if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion))
		{
			const FBox Box = Exclusion->Bounds->Bounds.GetBox().ExpandBy(FVector(Radius, Radius, HalfHeight));
			if (Box.IsInsideOrOn(SpawnLocation))
			{
				OutReason = TEXT("Spawn overlaps an authored exclusion region.");
				return false;
			}
		}
	}
	if (World->OverlapBlockingTestByProfile(SpawnLocation, Choice.Point->GetActorQuat(), Capsule->GetCollisionProfileName(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
	{
		OutReason = TEXT("Enemy capsule overlaps blocking geometry.");
		return false;
	}
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	const UCharacterMovementComponent* Movement = Defaults->GetCharacterMovement();
	const FNavAgentProperties* Agent = Movement ? &Movement->GetNavAgentPropertiesRef() : nullptr;
	const ANavigationData* NavData = Nav && Agent ? Nav->GetNavDataForProps(*Agent, SpawnLocation) : nullptr;
	FNavLocation OnNav;
	if (!NavData || !Nav->ProjectPointToNavigation(Ground, OnNav, FVector(Radius, Radius, HalfHeight), NavData)
		|| FVector::Dist2D(Ground, OnNav.Location) > MaxNavProjectionDistance
		|| FMath::Abs(Ground.Z - OnNav.Location.Z) > MaxNavProjectionHeight
		|| !RoomBounds.IsInsideOrOn(OnNav.Location))
	{
		OutReason = TEXT("No nearby navigation surface for this enemy's agent.");
		return false;
	}
	bool bReachable = false;
	for (const FVector& Entry : EntryLocations)
	{
		FNavLocation EntryOnNav;
		if (!Nav->ProjectPointToNavigation(Entry, EntryOnNav, FVector(Radius, Radius, HalfHeight), NavData)
			|| FVector::Dist2D(Entry, EntryOnNav.Location) > MaxNavProjectionDistance * 2.f
			|| FMath::Abs(Entry.Z - EntryOnNav.Location.Z) > MaxNavProjectionHeight * 2.f) continue;
		FPathFindingQuery PathQuery(this, *NavData, OnNav.Location, EntryOnNav.Location);
		PathQuery.SetAllowPartialPaths(false).SetNavAgentProperties(*Agent);
		const FPathFindingResult Path = Nav->FindPathSync(*Agent, PathQuery);
		if (Path.IsSuccessful() && Path.Path.IsValid() && !Path.Path->IsPartial())
		{
			bReachable = true;
			break;
		}
	}
	if (!bReachable)
	{
		OutReason = TEXT("Enemy has no complete navigation path to a room entry.");
		return false;
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.OverrideLevel = GetOwner()->GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	OutEnemy = World->SpawnActor<AEnemyCharacterBase>(Choice.EnemyClass, SpawnLocation, Choice.Point->GetActorRotation(), Params);
	if (!IsValid(OutEnemy))
	{
		OutReason = TEXT("SpawnActor was rejected by collision handling.");
		return false;
	}
	if (!OutEnemy->GetController()) OutEnemy->SpawnDefaultController();
	if (!OutEnemy->GetController() || !OutEnemy->GetEnemyASC() || OutEnemy->IsDead())
	{
		OutReason = TEXT("Spawned enemy has no AI controller, ASC or living state.");
		OutEnemy->Destroy();
		OutEnemy = nullptr;
		return false;
	}
	return true;
}

void UHBRoomEncounterComponent::HandleEnemyDied(AEnemyCharacterBase* Enemy)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SpawnedEnemies.Contains(Enemy) || CountedDeaths.Contains(Enemy)) return;
	CountedDeaths.Add(Enemy);
	AliveCount = FMath::Max(0, AliveCount - 1);
}

void UHBRoomEncounterComponent::ClearEncounter()
{
	for (AEnemyCharacterBase* Enemy : SpawnedEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnDied.RemoveAll(this);
			Enemy->Destroy();
		}
	}
	SpawnedEnemies.Reset();
	CountedDeaths.Reset();
	AliveCount = 0;
	ActiveRoomId = INDEX_NONE;
	ActiveGenerationId = INDEX_NONE;
	bPrepared = false;
}

void UHBRoomEncounterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearEncounter();
	Super::EndPlay(Reason);
}
