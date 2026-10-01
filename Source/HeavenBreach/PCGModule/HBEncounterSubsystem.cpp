#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationSystemTypes.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBEncounterSubsystem, Log, All);

namespace
{
	bool AllowsGrade(const AHBEnemySpawnPoint& Point, EHBEncounterGrade Grade)
	{
		switch (Grade)
		{
		case EHBEncounterGrade::Common: return Point.bAllowCommon;
		case EHBEncounterGrade::Elite: return Point.bAllowElite;
		case EHBEncounterGrade::Boss: return Point.bAllowBoss;
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
			Points.Swap(Index, Random.RandRange(0, Index));
	}

	bool TryOptionsAtPoint(const AHBEnemySpawnPoint& Point, const FHBEncounterGroup& Group,
		FRandomStream& Random, TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
		FHBEncounterSpawnChoice& OutChoice, FString& LastFailure)
	{
		TArray<FHBEncounterMonsterOption> Remaining;
		for (const FHBEncounterMonsterOption& Option : Group.Monsters)
			if (Option.EnemyClass && Option.Weight > 0 && AllowsClass(Point, Option.EnemyClass.Get()))
				Remaining.Add(Option);
		while (!Remaining.IsEmpty())
		{
			int32 TotalWeight = 0;
			for (const FHBEncounterMonsterOption& Option : Remaining) TotalWeight += Option.Weight;
			int32 Roll = Random.RandRange(1, TotalWeight);
			int32 Pick = 0;
			for (; Pick < Remaining.Num() - 1; ++Pick)
			{
				Roll -= Remaining[Pick].Weight;
				if (Roll <= 0) break;
			}
			FHBEncounterSpawnChoice Choice{Point.PointId, const_cast<AHBEnemySpawnPoint*>(&Point),
				Remaining[Pick].EnemyClass, Group.Grade};
			Remaining.RemoveAt(Pick);
			FString Reason;
			if (TryPlace(Choice, Reason)) { OutChoice = Choice; return true; }
			LastFailure = FString::Printf(TEXT("Point %s: %s"), *Point.PointId.ToString(), *Reason);
		}
		return false;
	}
}

bool HBEncounterSelection::SelectAndPlace(const UHBEncounterComponent& Config,
	const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
	TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError)
{
	OutPlaced.Reset();
	OutError.Reset();
	TArray<FString> ConfigErrors;
	if (!Config.Validate(ConfigErrors))
	{
		OutError = FString::Join(ConfigErrors, TEXT("; "));
		return false;
	}
	TArray<AHBEnemySpawnPoint*> SortedPoints = Points;
	SortedPoints.RemoveAll([](const AHBEnemySpawnPoint* Point) { return !IsValid(Point); });
	SortedPoints.Sort([](const AHBEnemySpawnPoint& A, const AHBEnemySpawnPoint& B)
	{
		return A.PointId.ToString() < B.PointId.ToString();
	});
	TSet<FName> AllIds;
	for (const AHBEnemySpawnPoint* Point : SortedPoints)
	{
		if (Point->PointId.IsNone() || AllIds.Contains(Point->PointId))
		{
			OutError = TEXT("Spawn Point IDs must be nonempty and unique within the room instance.");
			return false;
		}
		AllIds.Add(Point->PointId);
	}
	FRandomStream Random(MakeRoomSeed(DungeonSeed, RoomId));
	TSet<FName> UsedIds;
	FString LastFailure;
	for (EHBEncounterGrade Grade : {EHBEncounterGrade::Boss, EHBEncounterGrade::Elite, EHBEncounterGrade::Common})
	{
		for (const FHBEncounterGroup& Group : Config.Groups)
		{
			if (Group.Grade != Grade) continue;
			const int32 Target = Random.RandRange(Group.MinCount, Group.MaxCount);
			TArray<AHBEnemySpawnPoint*> Required;
			TArray<AHBEnemySpawnPoint*> Optional;
			for (AHBEnemySpawnPoint* Point : SortedPoints)
			{
				if (UsedIds.Contains(Point->PointId) || !AllowsGrade(*Point, Grade)
					|| (!Point->GroupId.IsNone() && Point->GroupId != Group.GroupId)) continue;
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
				if (TryOptionsAtPoint(*Point, Group, Random, TryPlace, Choice, LastFailure))
				{
					OutPlaced.Add(Choice);
					UsedIds.Add(Choice.PointId);
					++Placed;
				}
			}
			if (Placed < Group.MinCount)
			{
				OutError = FString::Printf(TEXT("Room %d group %s placed %d of required %d. %s"),
					RoomId, *Group.GroupId.ToString(), Placed, Group.MinCount, *LastFailure);
				return false;
			}
			if (Placed < Target)
				UE_LOG(LogHBEncounterSubsystem, Warning, TEXT("Room %d group %s placed %d of selected %d. %s"),
					RoomId, *Group.GroupId.ToString(), Placed, Target, *LastFailure);
		}
	}
	for (const AHBEnemySpawnPoint* Point : SortedPoints)
		if (Point->bRequired && !UsedIds.Contains(Point->PointId))
		{
			OutError = FString::Printf(TEXT("Room %d required point %s was not filled. %s"),
				RoomId, *Point->PointId.ToString(), *LastFailure);
			return false;
		}
	return true;
}

bool UHBEncounterSubsystem::HasEncounter(const ULevel* RoomLevel)
{
	if (!IsValid(RoomLevel)) return false;
	for (AActor* Actor : RoomLevel->Actors)
		if (IsValid(Actor) && Actor->GetLevel() == RoomLevel && Actor->IsA<AHBEncounterSetupActor>()) return true;
	return false;
}

bool UHBEncounterSubsystem::CanManageRoomSpawns() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

bool UHBEncounterSubsystem::StartEncounter(int32 GenerationId, int32 DungeonSeed, int32 RoomId,
	ULevel* RoomLevel, const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError)
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (!CanManageRoomSpawns())
	{
		OutError = TEXT("Only the server world may select or spawn encounters.");
		return false;
	}
	if (GenerationId <= 0 || RoomId < 0)
	{
		OutError = TEXT("Encounter requires a positive generation ID and nonnegative room ID.");
		return false;
	}
	if (!IsValid(RoomLevel) || RoomLevel->GetWorld() != World || !RoomBounds.IsValid || EntryLocations.IsEmpty())
	{
		OutError = TEXT("Encounter needs its loaded room level, valid bounds and an entry location.");
		return false;
	}
	for (const FRoomState& State : RoomStates)
	{
		if (State.GenerationId == GenerationId && State.DungeonSeed != DungeonSeed)
		{
			OutError = TEXT("An encounter generation cannot change its dungeon seed.");
			return false;
		}
		if (State.Level.Get() != RoomLevel && !(State.GenerationId == GenerationId && State.RoomId == RoomId)) continue;
		if (State.Level.Get() == RoomLevel && State.GenerationId == GenerationId && State.RoomId == RoomId && State.bPrepared) return true;
		OutError = TEXT("Encounter room identity conflicts with an existing generation or level; clear it before restarting.");
		return false;
	}
	AHBEncounterSetupActor* Setup = nullptr;
	TArray<AHBEnemySpawnPoint*> Points;
	for (AActor* Actor : RoomLevel->Actors)
	{
		if (!IsValid(Actor) || Actor->GetLevel() != RoomLevel) continue;
		if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor))
		{
			if (Setup) { OutError = TEXT("Room contains more than one Encounter Setup actor."); return false; }
			Setup = Candidate;
		}
		else if (auto* Point = Cast<AHBEnemySpawnPoint>(Actor)) Points.Add(Point);
	}
	if (!Setup)
	{
		if (!Points.IsEmpty()) { OutError = TEXT("Room has Spawn Points but no Encounter Setup actor."); return false; }
		return true;
	}
	if (!Setup->HasAuthority() || !IsValid(Setup->Encounter))
	{
		OutError = TEXT("Encounter Setup has no authority or settings component.");
		return false;
	}
	FRoomState& State = RoomStates.AddDefaulted_GetRef();
	State.Level = RoomLevel;
	State.GenerationId = GenerationId;
	State.RoomId = RoomId;
	State.DungeonSeed = DungeonSeed;
	TArray<FHBEncounterSpawnChoice> Placed;
	const bool bSucceeded = HBEncounterSelection::SelectAndPlace(*Setup->Encounter, Points, DungeonSeed, RoomId,
		[&](const FHBEncounterSpawnChoice& Choice, FString& Reason)
		{
			AEnemyCharacterBase* Enemy = nullptr;
			if (!TrySpawn(Choice, *Setup->Encounter, *Setup, *RoomLevel, RoomBounds, EntryLocations, State, Enemy, Reason)) return false;
			State.SpawnedEnemies.Add(Enemy);
			Enemy->OnDied.AddUObject(this, &UHBEncounterSubsystem::HandleEnemyDied);
			++State.AliveCount;
			UE_LOG(LogHBEncounterSubsystem, Display, TEXT("Room %d point %s spawned %s at %s."),
				RoomId, *Choice.PointId.ToString(), *GetNameSafe(Choice.EnemyClass.Get()),
				*Enemy->GetActorLocation().ToString());
			return true;
		}, Placed, OutError);
	if (!bSucceeded)
	{
		ClearRoom(State);
		RoomStates.Pop();
		return false;
	}
	State.bPrepared = true;
	UE_LOG(LogHBEncounterSubsystem, Display, TEXT("Room %d encounter ready: %d enemies, seed %d."),
		RoomId, State.SpawnedEnemies.Num(), DungeonSeed);
	return true;
}

bool UHBEncounterSubsystem::IsPreparedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	if (!CanManageRoomSpawns() || !IsValid(RoomLevel) || GenerationId <= 0 || RoomId < 0) return false;
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId
			&& State.Level.Get() == RoomLevel && State.bPrepared) return true;
	return false;
}

int32 UHBEncounterSubsystem::GetAliveCount(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	if (!CanManageRoomSpawns() || !IsValid(RoomLevel) || GenerationId <= 0 || RoomId < 0) return 0;
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId && State.Level.Get() == RoomLevel)
			return State.AliveCount;
	return 0;
}

bool UHBEncounterSubsystem::TrySpawn(const FHBEncounterSpawnChoice& Choice, const UHBEncounterComponent& Config,
	AHBEncounterSetupActor& Setup, ULevel& RoomLevel, const FBox& RoomBounds,
	const TArray<FVector>& EntryLocations, const FRoomState& State,
	AEnemyCharacterBase*& OutEnemy, FString& OutReason)
{
	OutEnemy = nullptr;
	UWorld* World = GetWorld();
	const auto* Defaults = Choice.EnemyClass ? Choice.EnemyClass->GetDefaultObject<AEnemyCharacterBase>() : nullptr;
	const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	if (!CanManageRoomSpawns() || !IsValid(Choice.Point) || !Capsule || Choice.Point->GetLevel() != &RoomLevel)
	{
		OutReason = TEXT("Missing world, room-owned point or enemy capsule.");
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
		|| Floor.ImpactNormal.Z < Config.MinFloorUpDot)
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
		if (FVector::Dist2D(Ground, Entry) < Config.MinEntryDistance + Radius)
		{
			OutReason = TEXT("Spawn blocks a room entry or player start.");
			return false;
		}
	for (const TWeakObjectPtr<AEnemyCharacterBase>& OtherPtr : State.SpawnedEnemies)
		if (AEnemyCharacterBase* Other = OtherPtr.Get())
		{
			const float OtherRadius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
			if (FVector::Dist2D(SpawnLocation, Other->GetActorLocation()) < Radius + OtherRadius + Config.MinMonsterSpacing)
			{
				OutReason = TEXT("Spawn is too close to another monster.");
				return false;
			}
		}
	for (AActor* Actor : RoomLevel.Actors)
		if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion))
		{
			const FBox Box = Exclusion->Bounds->Bounds.GetBox().ExpandBy(FVector(Radius, Radius, HalfHeight));
			if (Box.IsInsideOrOn(SpawnLocation))
			{
				OutReason = TEXT("Spawn overlaps an authored exclusion region.");
				return false;
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
		|| FVector::Dist2D(Ground, OnNav.Location) > Config.MaxNavProjectionDistance
		|| FMath::Abs(Ground.Z - OnNav.Location.Z) > Config.MaxNavProjectionHeight
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
			|| FVector::Dist2D(Entry, EntryOnNav.Location) > Config.MaxNavProjectionDistance * 2.f
			|| FMath::Abs(Entry.Z - EntryOnNav.Location.Z) > Config.MaxNavProjectionHeight * 2.f) continue;
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
	Params.Owner = &Setup;
	Params.OverrideLevel = &RoomLevel;
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

void UHBEncounterSubsystem::HandleEnemyDied(AEnemyCharacterBase* Enemy)
{
	if (!CanManageRoomSpawns()) return;
	for (FRoomState& State : RoomStates)
		if (State.SpawnedEnemies.Contains(Enemy) && !State.CountedDeaths.Contains(Enemy))
		{
			State.CountedDeaths.Add(Enemy);
			State.AliveCount = FMath::Max(0, State.AliveCount - 1);
			return;
		}
}

void UHBEncounterSubsystem::ClearRoom(FRoomState& State)
{
	if (!CanManageRoomSpawns()) return;
	for (const TWeakObjectPtr<AEnemyCharacterBase>& EnemyPtr : State.SpawnedEnemies)
		if (AEnemyCharacterBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDied.RemoveAll(this);
			Enemy->Destroy();
		}
	State.SpawnedEnemies.Reset();
	State.CountedDeaths.Reset();
	State.AliveCount = 0;
	State.bPrepared = false;
}

void UHBEncounterSubsystem::ClearGeneration(int32 GenerationId)
{
	if (!CanManageRoomSpawns() || GenerationId <= 0) return;
	for (int32 Index = RoomStates.Num() - 1; Index >= 0; --Index)
		if (RoomStates[Index].GenerationId == GenerationId)
		{
			ClearRoom(RoomStates[Index]);
			RoomStates.RemoveAt(Index);
		}
}

void UHBEncounterSubsystem::Deinitialize()
{
	for (FRoomState& State : RoomStates) ClearRoom(State);
	RoomStates.Reset();
	Super::Deinitialize();
}
