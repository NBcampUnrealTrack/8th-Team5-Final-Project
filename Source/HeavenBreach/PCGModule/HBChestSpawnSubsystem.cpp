#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "Loot/BoxActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBChestSpawn, Log, All);

namespace HBChestSpawnGeometry
{
	bool MakeGroundedTransform(const UClass* ChestClass, const FTransform& PointTransform,
		float FloorZ, FTransform& OutTransform, FString& OutError)
	{
		OutError.Reset();
		const ABoxActor* Defaults = ChestClass ? ChestClass->GetDefaultObject<ABoxActor>() : nullptr;
		const UStaticMeshComponent* Mesh = Defaults
			? Cast<UStaticMeshComponent>(Defaults->GetRootComponent()) : nullptr;
		if (!Mesh || !Mesh->GetStaticMesh())
		{
			OutError = TEXT("Box Blueprint needs a static mesh on its root component.");
			return false;
		}
		const FTransform MeshTransform = Mesh->GetRelativeTransform() * PointTransform;
		const FBoxSphereBounds Bounds = Mesh->CalcBounds(MeshTransform);
		const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
		if (!FMath::IsFinite(BottomZ) || !FMath::IsFinite(FloorZ))
		{
			OutError = TEXT("Box mesh or floor bounds are not finite.");
			return false;
		}
		constexpr float FloorClearance = 2.f;
		OutTransform = PointTransform;
		OutTransform.AddToTranslation(FVector(0, 0, FloorZ + FloorClearance - BottomZ));
		return true;
	}
}

namespace
{
	int32 MakeChestRoomSeed(int32 DungeonSeed, int32 RoomId)
	{
		return static_cast<int32>(HashCombineFast(HashCombineFast(
			static_cast<uint32>(DungeonSeed), static_cast<uint32>(RoomId)), 0x48424348u));
	}

	void ShufflePoints(TArray<AHBChestSpawnPoint*>& Points, FRandomStream& Random)
	{
		for (int32 Index = Points.Num() - 1; Index > 0; --Index)
			Points.Swap(Index, Random.RandRange(0, Index));
	}

	bool AllowsClass(const AHBChestSpawnPoint& Point, UClass* ChestClass)
	{
		return Point.AllowedClasses.IsEmpty() || Point.AllowedClasses.ContainsByPredicate(
			[ChestClass](const TSubclassOf<ABoxActor>& Allowed) { return Allowed.Get() == ChestClass; });
	}

	bool TryOptionsAtPoint(const AHBChestSpawnPoint& Point, const FHBChestPlacementGroup& Group,
		FRandomStream& Random, TFunctionRef<bool(const FHBChestSpawnChoice&, FString&)> TryPlace,
		FHBChestSpawnChoice& OutChoice, FString& LastFailure)
	{
		TArray<FHBChestClassOption> Remaining;
		for (const FHBChestClassOption& Option : Group.Chests)
			if (Option.ChestClass && Option.Weight > 0 && AllowsClass(Point, Option.ChestClass.Get()))
				Remaining.Add(Option);
		while (!Remaining.IsEmpty())
		{
			int32 TotalWeight = 0;
			for (const FHBChestClassOption& Option : Remaining) TotalWeight += Option.Weight;
			int32 Roll = Random.RandRange(1, TotalWeight);
			int32 Pick = 0;
			for (; Pick < Remaining.Num() - 1; ++Pick)
			{
				Roll -= Remaining[Pick].Weight;
				if (Roll <= 0) break;
			}
			FHBChestSpawnChoice Choice{Point.PointId, const_cast<AHBChestSpawnPoint*>(&Point),
				Remaining[Pick].ChestClass};
			Remaining.RemoveAt(Pick);
			FString Reason;
			if (TryPlace(Choice, Reason)) { OutChoice = Choice; return true; }
			LastFailure = FString::Printf(TEXT("Point %s: %s"), *Point.PointId.ToString(), *Reason);
		}
		return false;
	}
}

bool HBChestSelection::SelectAndPlace(const UHBChestPlacementComponent& Config,
	const TArray<AHBChestSpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBChestSpawnChoice&, FString&)> TryPlace,
	TArray<FHBChestSpawnChoice>& OutPlaced, FString& OutError)
{
	OutPlaced.Reset();
	OutError.Reset();
	TArray<FString> ConfigErrors;
	if (!Config.Validate(ConfigErrors))
	{
		OutError = FString::Join(ConfigErrors, TEXT("; "));
		return false;
	}
	TArray<AHBChestSpawnPoint*> SortedPoints = Points;
	SortedPoints.RemoveAll([](const AHBChestSpawnPoint* Point) { return !IsValid(Point); });
	SortedPoints.Sort([](const AHBChestSpawnPoint& A, const AHBChestSpawnPoint& B)
	{
		return A.PointId.ToString() < B.PointId.ToString();
	});
	TSet<FName> AllIds;
	for (const AHBChestSpawnPoint* Point : SortedPoints)
	{
		if (Point->PointId.IsNone() || AllIds.Contains(Point->PointId))
		{
			OutError = TEXT("Chest Spawn Point IDs must be nonempty and unique within the room instance.");
			return false;
		}
		AllIds.Add(Point->PointId);
	}
	FRandomStream Random(MakeChestRoomSeed(DungeonSeed, RoomId));
	TSet<FName> UsedIds;
	for (const FHBChestPlacementGroup& Group : Config.Groups)
	{
		const int32 Target = Random.RandRange(Group.MinCount, Group.MaxCount);
		TArray<AHBChestSpawnPoint*> Required;
		TArray<AHBChestSpawnPoint*> Optional;
		for (AHBChestSpawnPoint* Point : SortedPoints)
		{
			if (UsedIds.Contains(Point->PointId)
				|| (!Point->GroupId.IsNone() && Point->GroupId != Group.GroupId)) continue;
			(Point->bRequired ? Required : Optional).Add(Point);
		}
		ShufflePoints(Required, Random);
		ShufflePoints(Optional, Random);
		Required.Append(Optional);
		int32 Placed = 0;
		FString LastFailure;
		for (AHBChestSpawnPoint* Point : Required)
		{
			if (Placed >= Target) break;
			FHBChestSpawnChoice Choice;
			if (TryOptionsAtPoint(*Point, Group, Random, TryPlace, Choice, LastFailure))
			{
				OutPlaced.Add(Choice);
				UsedIds.Add(Choice.PointId);
				++Placed;
			}
		}
		if (Placed < Group.MinCount)
		{
			OutError = FString::Printf(TEXT("Room %d chest group %s placed %d of required %d. %s"),
				RoomId, *Group.GroupId.ToString(), Placed, Group.MinCount, *LastFailure);
			return false;
		}
		if (Placed < Target)
			UE_LOG(LogHBChestSpawn, Warning, TEXT("Room %d chest group %s placed %d of selected %d. %s"),
				RoomId, *Group.GroupId.ToString(), Placed, Target, *LastFailure);
	}
	for (const AHBChestSpawnPoint* Point : SortedPoints)
		if (Point->bRequired && !UsedIds.Contains(Point->PointId))
		{
			OutError = FString::Printf(TEXT("Room %d required chest point %s was not filled."),
				RoomId, *Point->PointId.ToString());
			return false;
		}
	return true;
}

bool UHBChestSpawnSubsystem::HasPlacement(const ULevel* RoomLevel)
{
	if (!IsValid(RoomLevel)) return false;
	for (AActor* Actor : RoomLevel->Actors)
		if (IsValid(Actor) && Actor->GetLevel() == RoomLevel && Actor->IsA<AHBChestSetupActor>()) return true;
	return false;
}

bool UHBChestSpawnSubsystem::StartPlacement(int32 GenerationId, int32 DungeonSeed, int32 RoomId,
	ULevel* RoomLevel, const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError)
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Only the server world may select or spawn room chests.");
		return false;
	}
	if (!IsValid(RoomLevel) || RoomLevel->GetWorld() != World || !RoomBounds.IsValid || EntryLocations.IsEmpty())
	{
		OutError = TEXT("Chest placement needs its loaded room level, valid bounds and an entry location.");
		return false;
	}
	AHBChestSetupActor* Setup = nullptr;
	TArray<AHBChestSpawnPoint*> Points;
	for (AActor* Actor : RoomLevel->Actors)
	{
		if (!IsValid(Actor) || Actor->GetLevel() != RoomLevel) continue;
		if (auto* Candidate = Cast<AHBChestSetupActor>(Actor))
		{
			if (Setup) { OutError = TEXT("Room contains more than one Chest Setup actor."); return false; }
			Setup = Candidate;
		}
		else if (auto* Point = Cast<AHBChestSpawnPoint>(Actor)) Points.Add(Point);
	}
	if (!Setup)
	{
		if (!Points.IsEmpty()) { OutError = TEXT("Room has Chest Spawn Points but no Chest Setup actor."); return false; }
		return true;
	}
	if (!Setup->HasAuthority() || !IsValid(Setup->Placement))
	{
		OutError = TEXT("Chest Setup has no authority or settings component.");
		return false;
	}
	for (const FRoomState& State : RoomStates)
	{
		if (State.Level.Get() != RoomLevel) continue;
		if (State.GenerationId == GenerationId && State.RoomId == RoomId && State.bPrepared) return true;
		OutError = TEXT("Room chests belong to another generation; clear them before restarting.");
		return false;
	}
	FRoomState& State = RoomStates.AddDefaulted_GetRef();
	State.Level = RoomLevel;
	State.GenerationId = GenerationId;
	State.RoomId = RoomId;
	TArray<FHBChestSpawnChoice> Placed;
	const bool bSucceeded = HBChestSelection::SelectAndPlace(*Setup->Placement, Points, DungeonSeed, RoomId,
		[&](const FHBChestSpawnChoice& Choice, FString& Reason)
		{
			ABoxActor* Chest = nullptr;
			if (!TrySpawn(Choice, *Setup->Placement, *Setup, *RoomLevel, RoomBounds,
				EntryLocations, State, Chest, Reason)) return false;
			State.SpawnedChests.Add(Chest);
			UE_LOG(LogHBChestSpawn, Display, TEXT("Room %d point %s spawned %s at %s."),
				RoomId, *Choice.PointId.ToString(), *GetNameSafe(Choice.ChestClass.Get()),
				*Chest->GetActorLocation().ToString());
			return true;
		}, Placed, OutError);
	if (!bSucceeded)
	{
		ClearRoom(State);
		RoomStates.Pop();
		return false;
	}
	State.bPrepared = true;
	UE_LOG(LogHBChestSpawn, Display, TEXT("Room %d chests ready: %d, seed %d."),
		RoomId, State.SpawnedChests.Num(), DungeonSeed);
	return true;
}

bool UHBChestSpawnSubsystem::IsPreparedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId
			&& State.Level.Get() == RoomLevel && State.bPrepared) return true;
	return false;
}

int32 UHBChestSpawnSubsystem::GetSpawnedCount(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId && State.Level.Get() == RoomLevel)
		{
			int32 Count = 0;
			for (const TWeakObjectPtr<ABoxActor>& Chest : State.SpawnedChests)
				if (IsValid(Chest.Get())) ++Count;
			return Count;
		}
	return 0;
}

void UHBChestSpawnSubsystem::ClearRoom(FRoomState& State)
{
	for (const TWeakObjectPtr<ABoxActor>& Chest : State.SpawnedChests)
		if (IsValid(Chest.Get())) Chest->Destroy();
	State.SpawnedChests.Reset();
	State.bPrepared = false;
}

void UHBChestSpawnSubsystem::ClearGeneration(int32 GenerationId)
{
	for (int32 Index = RoomStates.Num() - 1; Index >= 0; --Index)
		if (RoomStates[Index].GenerationId == GenerationId)
		{
			ClearRoom(RoomStates[Index]);
			RoomStates.RemoveAt(Index);
		}
}

void UHBChestSpawnSubsystem::Deinitialize()
{
	for (FRoomState& State : RoomStates) ClearRoom(State);
	RoomStates.Reset();
	Super::Deinitialize();
}

bool UHBChestSpawnSubsystem::TrySpawn(const FHBChestSpawnChoice& Choice, const UHBChestPlacementComponent& Config,
	AHBChestSetupActor& Setup, ULevel& RoomLevel, const FBox& RoomBounds,
	const TArray<FVector>& EntryLocations, const FRoomState& State,
	ABoxActor*& OutChest, FString& OutReason)
{
	OutChest = nullptr;
	UWorld* World = GetWorld();
	if (!World || !IsValid(Choice.Point) || !Choice.ChestClass || Choice.Point->GetLevel() != &RoomLevel)
	{
		OutReason = TEXT("Missing world, room-owned point or box class.");
		return false;
	}
	const FVector Marker = Choice.Point->GetActorLocation();
	if (!RoomBounds.IsInsideOrOn(Marker))
	{
		OutReason = TEXT("Chest point is outside the room bounds.");
		return false;
	}
	FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(HBChestFloor), false);
	FloorQuery.AddIgnoredActor(Choice.Point);
	for (const TWeakObjectPtr<ABoxActor>& Other : State.SpawnedChests)
		if (IsValid(Other.Get())) FloorQuery.AddIgnoredActor(Other.Get());
	FHitResult Floor;
	if (!World->LineTraceSingleByChannel(Floor, Marker + FVector(0, 0, 100),
		Marker - FVector(0, 0, 200), ECC_WorldStatic, FloorQuery)
		|| Floor.ImpactNormal.Z < Config.MinFloorUpDot)
	{
		OutReason = TEXT("No safe floor below the chest point.");
		return false;
	}
	const FVector Ground = Floor.ImpactPoint;
	if (!RoomBounds.IsInsideOrOn(Ground))
	{
		OutReason = TEXT("Chest floor is outside the room bounds.");
		return false;
	}
	for (const FVector& Entry : EntryLocations)
		if (FVector::Dist2D(Marker, Entry) < Config.MinEntryDistance)
		{
			OutReason = TEXT("Chest is too close to a room entry or player start.");
			return false;
		}
	for (const TWeakObjectPtr<ABoxActor>& Other : State.SpawnedChests)
		if (IsValid(Other.Get()) && FVector::Dist2D(Marker, Other->GetActorLocation()) < Config.MinChestSpacing)
		{
			OutReason = TEXT("Chest is too close to another chest.");
			return false;
		}
	for (AActor* Actor : RoomLevel.Actors)
		if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion)
			&& Exclusion->Bounds->Bounds.GetBox().ExpandBy(FVector(60, 60, 100)).IsInsideOrOn(Marker))
		{
			OutReason = TEXT("Chest overlaps an authored spawn exclusion region.");
			return false;
		}
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	const AGameModeBase* Mode = World->GetAuthGameMode();
	const ACharacter* PlayerDefaults = Mode && Mode->DefaultPawnClass
		? Cast<ACharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
	const UCharacterMovementComponent* Movement = PlayerDefaults ? PlayerDefaults->GetCharacterMovement() : nullptr;
	const FNavAgentProperties* Agent = Movement ? &Movement->GetNavAgentPropertiesRef() : nullptr;
	const ANavigationData* NavData = Nav && Agent ? Nav->GetNavDataForProps(*Agent, Ground) : nullptr;
	if (!NavData || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
	{
		OutReason = TEXT("Player navigation is not ready for this chest.");
		return false;
	}
	const FVector Approach = Ground + Choice.Point->GetActorForwardVector() * 120.f;
	FNavLocation ApproachOnNav;
	if (!Nav->ProjectPointToNavigation(Approach, ApproachOnNav, FVector(100, 100, 200), NavData)
		|| FVector::Dist2D(Approach, ApproachOnNav.Location) > Config.MaxNavProjectionDistance
		|| FMath::Abs(Approach.Z - ApproachOnNav.Location.Z) > Config.MaxNavProjectionHeight
		|| !RoomBounds.IsInsideOrOn(ApproachOnNav.Location))
	{
		OutReason = TEXT("No nearby navigation surface at the chest approach.");
		return false;
	}
	bool bReachable = false;
	for (const FVector& Entry : EntryLocations)
	{
		FNavLocation EntryOnNav;
		if (!Nav->ProjectPointToNavigation(Entry, EntryOnNav, FVector(100, 100, 200), NavData)) continue;
		FPathFindingQuery PathQuery(this, *NavData, EntryOnNav.Location, ApproachOnNav.Location);
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
		OutReason = TEXT("Player has no complete navigation path to the chest approach.");
		return false;
	}
	FActorSpawnParameters Params;
	Params.Owner = &Setup;
	Params.OverrideLevel = &RoomLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	FTransform SpawnTransform;
	if (!HBChestSpawnGeometry::MakeGroundedTransform(Choice.ChestClass.Get(),
		Choice.Point->GetActorTransform(), Ground.Z, SpawnTransform, OutReason)) return false;
	OutChest = World->SpawnActor<ABoxActor>(Choice.ChestClass, SpawnTransform, Params);
	if (!IsValid(OutChest))
	{
		OutReason = FString::Printf(TEXT("Box %s at %s was blocked after floor alignment (floor Z=%.1f)."),
			*GetNameSafe(Choice.ChestClass.Get()), *SpawnTransform.GetLocation().ToString(), Ground.Z);
		return false;
	}
	if (OutChest->GetLevel() != &RoomLevel)
	{
		OutChest->Destroy();
		OutChest = nullptr;
		OutReason = TEXT("Spawned box did not belong to its source room level.");
		return false;
	}
	return true;
}
