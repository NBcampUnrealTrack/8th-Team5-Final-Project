#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBSpawnGroundQuery.h"
#include "PCGModule/HBSpawnDiagnostics.h"
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
		FRandomStream& Random, TFunctionRef<bool(const FHBChestSpawnChoice&, FString&, FHBSpawnPlacementReport&)> TryPlace,
		FHBChestSpawnChoice& OutChoice, FHBSpawnSelectionReport& Report)
	{
		TArray<FHBChestClassOption> Remaining;
		for (const FHBChestClassOption& Option : Group.Chests)
			if (Option.ChestClass && Option.Weight > 0 && AllowsClass(Point, Option.ChestClass.Get()))
				Remaining.Add(Option);
		if (Remaining.IsEmpty())
			Report.RecordSkip(Point.PointId, Group.GroupId, EHBSpawnFailureReason::NoCompatibleClass);
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
				Remaining[Pick].ChestClass, Group.bRequireEntryPath};
			Remaining.RemoveAt(Pick);
			FString Reason;
			FHBSpawnPlacementReport PlacementReport;
			if (TryPlace(Choice, Reason, PlacementReport))
			{
				Report.RecordPlaced(Point.PointId, Group.GroupId, Choice.ChestClass->GetPathName(), &PlacementReport);
				OutChoice = Choice;
				return true;
			}
			Report.RecordClassFailure(Point.PointId, Group.GroupId, Choice.ChestClass->GetPathName(), Reason, &PlacementReport);
		}
		return false;
	}
}

bool HBChestSelection::SelectAndPlace(const UHBChestPlacementComponent& Config,
	const TArray<AHBChestSpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBChestSpawnChoice&, FString&)> TryPlace,
	TArray<FHBChestSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport)
{
	return SelectAndPlaceWithDiagnostics(Config, Points, DungeonSeed, RoomId,
		[&](const FHBChestSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport&)
		{ return TryPlace(Choice, Reason); }, OutPlaced, OutError, OutReport);
}

bool HBChestSelection::SelectAndPlaceWithDiagnostics(const UHBChestPlacementComponent& Config,
	const TArray<AHBChestSpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBChestSpawnChoice&, FString&, FHBSpawnPlacementReport&)> TryPlace,
	TArray<FHBChestSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport)
{
	OutPlaced.Reset();
	OutError.Reset();
	FHBSpawnSelectionReport LocalReport;
	FHBSpawnSelectionReport& Report = OutReport ? *OutReport : LocalReport;
	Report = FHBSpawnSelectionReport();
	Report.RoomId = RoomId;
	Report.DungeonSeed = DungeonSeed;
	TArray<FString> ConfigErrors;
	if (!Config.Validate(ConfigErrors))
	{
		OutError = FString::Join(ConfigErrors, TEXT("; "));
		Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
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
			Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
			return false;
		}
		AllIds.Add(Point->PointId);
		Report.AddPoint(Point->PointId, Point->bRequired);
	}
	FRandomStream Random(MakeChestRoomSeed(DungeonSeed, RoomId));
	TSet<FName> UsedIds;
	for (const FHBChestPlacementGroup& Group : Config.Groups)
	{
		int32 Target = 0;
		if (!HBSpawnCountSelection::Select(Group.MinCount, Group.MaxCount, Group.CountSelectionMode,
			Group.CountWeights, Random, Target, OutError))
		{
			Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
			return false;
		}
		FHBSpawnGroupReport& GroupReport = Report.Groups.AddDefaulted_GetRef();
		GroupReport.GroupId = Group.GroupId;
		GroupReport.MinCount = Group.MinCount;
		GroupReport.Target = Target;
		TArray<AHBChestSpawnPoint*> Required;
		TArray<AHBChestSpawnPoint*> Optional;
		for (AHBChestSpawnPoint* Point : SortedPoints)
		{
			if (UsedIds.Contains(Point->PointId)) continue;
			if (!Point->GroupId.IsNone() && Point->GroupId != Group.GroupId)
			{
				Report.RecordSkip(Point->PointId, Group.GroupId, EHBSpawnFailureReason::GroupMismatch);
				continue;
			}
			(Point->bRequired ? Required : Optional).Add(Point);
		}
		ShufflePoints(Required, Random);
		ShufflePoints(Optional, Random);
		Required.Append(Optional);
		int32 Placed = 0;
		for (int32 PointIndex = 0; PointIndex < Required.Num(); ++PointIndex)
		{
			if (Placed >= Target)
			{
				for (int32 SkippedIndex = PointIndex; SkippedIndex < Required.Num(); ++SkippedIndex)
					Report.RecordSkip(Required[SkippedIndex]->PointId, Group.GroupId, EHBSpawnFailureReason::TargetReached);
				break;
			}
			AHBChestSpawnPoint* Point = Required[PointIndex];
			FHBChestSpawnChoice Choice;
			if (TryOptionsAtPoint(*Point, Group, Random, TryPlace, Choice, Report))
			{
				OutPlaced.Add(Choice);
				UsedIds.Add(Choice.PointId);
				Report.RecordPlaced(Choice.PointId);
				++Placed;
			}
		}
		GroupReport.Placed = Placed;
		if (Placed < Group.MinCount)
		{
			Report.FailureReason = GroupReport.FailureReason = EHBSpawnFailureReason::MinCountNotMet;
			Report.FailedGroupId = Group.GroupId;
			OutError = FString::Printf(TEXT("Room %d chest group %s placed %d of required %d. FailureReason=MinCountNotMet Target=%d Seed=%d %s"),
				RoomId, *Group.GroupId.ToString(), Placed, Group.MinCount, Target, DungeonSeed,
				*Report.FormatGroupResults(Group.GroupId));
			return false;
		}
		if (Placed < Target)
			UE_LOG(LogHBChestSpawn, Warning, TEXT("Room %d chest group %s placed %d of selected %d. %s"),
				RoomId, *Group.GroupId.ToString(), Placed, Target, *Report.FormatGroupResults(Group.GroupId));
	}
	for (const AHBChestSpawnPoint* Point : SortedPoints)
		if (Point->bRequired && !UsedIds.Contains(Point->PointId))
		{
			Report.FailureReason = EHBSpawnFailureReason::RequiredPointUnfilled;
			Report.FailedPointId = Point->PointId;
			OutError = Report.FormatRequiredFailure(Point->PointId, true);
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

bool UHBChestSpawnSubsystem::CanManageRoomSpawns() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

bool UHBChestSpawnSubsystem::StartPlacement(int32 GenerationId, int32 DungeonSeed, int32 RoomId,
	ULevel* RoomLevel, const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError)
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (!CanManageRoomSpawns())
	{
		OutError = TEXT("Only the server world may select or spawn room chests.");
		return false;
	}
	if (GenerationId <= 0 || RoomId < 0)
	{
		OutError = TEXT("Chest placement requires a positive generation ID and nonnegative room ID.");
		return false;
	}
	if (!IsValid(RoomLevel) || RoomLevel->GetWorld() != World || !RoomBounds.IsValid || EntryLocations.IsEmpty())
	{
		OutError = TEXT("Chest placement needs its loaded room level, valid bounds and an entry location.");
		return false;
	}
	for (const FRoomState& State : RoomStates)
	{
		if (State.GenerationId == GenerationId && State.DungeonSeed != DungeonSeed)
		{
			OutError = TEXT("A chest generation cannot change its dungeon seed.");
			return false;
		}
		if (State.Level.Get() != RoomLevel && !(State.GenerationId == GenerationId && State.RoomId == RoomId)) continue;
		if (State.Level.Get() == RoomLevel && State.GenerationId == GenerationId && State.RoomId == RoomId && State.bPrepared) return true;
		OutError = TEXT("Chest room identity conflicts with an existing generation or level; clear it before restarting.");
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
	FRoomState& State = RoomStates.AddDefaulted_GetRef();
	State.Level = RoomLevel;
	State.GenerationId = GenerationId;
	State.RoomId = RoomId;
	State.DungeonSeed = DungeonSeed;
	TArray<FHBChestSpawnChoice> Placed;
	const bool bSucceeded = HBChestSelection::SelectAndPlaceWithDiagnostics(*Setup->Placement, Points, DungeonSeed, RoomId,
		[&](const FHBChestSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport& PlacementReport)
		{
			ABoxActor* Chest = nullptr;
			if (!TrySpawn(Choice, *Setup->Placement, *Setup, *RoomLevel, RoomBounds,
				EntryLocations, State, Chest, Reason, PlacementReport)) return false;
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
	if (!CanManageRoomSpawns() || !IsValid(RoomLevel) || GenerationId <= 0 || RoomId < 0) return false;
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId
			&& State.Level.Get() == RoomLevel && State.bPrepared) return true;
	return false;
}

int32 UHBChestSpawnSubsystem::GetSpawnedCount(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	if (!CanManageRoomSpawns() || !IsValid(RoomLevel) || GenerationId <= 0 || RoomId < 0) return 0;
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
	if (!CanManageRoomSpawns()) return;
	for (const TWeakObjectPtr<ABoxActor>& Chest : State.SpawnedChests)
		if (IsValid(Chest.Get())) Chest->Destroy();
	State.SpawnedChests.Reset();
	State.bPrepared = false;
}

void UHBChestSpawnSubsystem::ClearGeneration(int32 GenerationId)
{
	if (!CanManageRoomSpawns() || GenerationId <= 0) return;
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
	ABoxActor*& OutChest, FString& OutReason, FHBSpawnPlacementReport& PlacementReport)
{
	OutChest = nullptr;
	UWorld* World = GetWorld();
	if (!CanManageRoomSpawns() || !IsValid(Choice.Point) || !Choice.ChestClass || Choice.Point->GetLevel() != &RoomLevel)
	{
		OutReason = TEXT("Missing world, room-owned point or box class.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidConfiguration, OutReason);
		return false;
	}
	const FVector Marker = Choice.Point->GetActorLocation();
	if (!RoomBounds.IsInsideOrOn(Marker))
	{
		OutReason = TEXT("Chest point is outside the room bounds.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::OutsideRoom, OutReason);
		return false;
	}
	FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(HBChestFloor), false);
	FloorQuery.AddIgnoredActor(Choice.Point);
	for (const TWeakObjectPtr<ABoxActor>& Other : State.SpawnedChests)
		if (IsValid(Other.Get())) FloorQuery.AddIgnoredActor(Other.Get());
	FHitResult Floor;
	if (!HBSpawnGroundQuery::FindPlacementWithDiagnostics(*World, Marker, Choice.Point->GetActorQuat(), RoomBounds,
		Config.MinFloorUpDot, FloorQuery,
		[&](const FHitResult& CandidateFloor, FString& Reason, FHBSpawnCandidateReport& Evidence)
		{
			const FVector Ground = CandidateFloor.ImpactPoint;
			const FVector CandidateMarker(Ground.X, Ground.Y, Marker.Z);
			if (!RoomBounds.IsInsideOrOn(Ground))
			{
				Reason = TEXT("Chest floor is outside the room bounds.");
				Evidence.Cause = EHBSpawnFailureReason::OutsideRoom;
				return false;
			}
			for (const FVector& Entry : EntryLocations)
				if (FVector::Dist2D(CandidateMarker, Entry) < Config.MinEntryDistance)
				{
					Reason = TEXT("Chest is too close to a room entry or player start.");
					Evidence.Cause = EHBSpawnFailureReason::EntrySpacing;
					return false;
				}
			for (const TWeakObjectPtr<ABoxActor>& Other : State.SpawnedChests)
				if (IsValid(Other.Get()) && FVector::Dist2D(CandidateMarker, Other->GetActorLocation()) < Config.MinChestSpacing)
				{
					Reason = TEXT("Chest is too close to another chest.");
					Evidence.Cause = EHBSpawnFailureReason::ChestSpacing;
					return false;
				}
			for (AActor* Actor : RoomLevel.Actors)
				if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion)
					&& Exclusion->Bounds->Bounds.GetBox().ExpandBy(FVector(60, 60, 100)).IsInsideOrOn(CandidateMarker))
				{
					Reason = TEXT("Chest overlaps an authored spawn exclusion region.");
					Evidence.Cause = EHBSpawnFailureReason::ExcludedArea;
					return false;
				}
			UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
			const AGameModeBase* Mode = World->GetAuthGameMode();
			const ACharacter* PlayerDefaults = Mode && Mode->DefaultPawnClass
				? Cast<ACharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
			const UCharacterMovementComponent* Movement = PlayerDefaults ? PlayerDefaults->GetCharacterMovement() : nullptr;
			const FNavAgentProperties* Agent = Movement ? &Movement->GetNavAgentPropertiesRef() : nullptr;
			const ANavigationData* NavData = Nav && Agent ? Nav->GetNavDataForProps(*Agent, Ground) : nullptr;
			const FVector Approach = Ground + Choice.Point->GetActorForwardVector() * 120.f;
			FNavLocation ApproachOnNav;
			FHBSpawnNavCheck& ApproachCheck = Evidence.NavigationChecks.AddDefaulted_GetRef();
			ApproachCheck.Stage = EHBSpawnNavStage::ChestApproach;
			ApproachCheck.bEntryToPlacement = true;
			if (!HBSpawnDiagnostics::ProjectNavigation(*World, Nav, Agent, NavData, Approach,
				FVector(100, 100, 200), Config.MaxNavProjectionDistance, Config.MaxNavProjectionHeight,
				&RoomBounds, true, ApproachOnNav, ApproachCheck))
			{
				Reason = ApproachCheck.bProjectionQueried ? TEXT("No nearby navigation surface at the chest approach.")
					: TEXT("Player navigation is not ready for this chest.");
				Evidence.Cause = EHBSpawnFailureReason::NavProjectionFailed;
				return false;
			}
			if (Choice.bRequireEntryPath)
			{
				bool bReachable = false;
				for (int32 EntryIndex = 0; EntryIndex < EntryLocations.Num(); ++EntryIndex)
				{
					const FVector& Entry = EntryLocations[EntryIndex];
					FNavLocation EntryOnNav;
					FHBSpawnNavCheck& EntryCheck = Evidence.NavigationChecks.AddDefaulted_GetRef();
					EntryCheck.Stage = EHBSpawnNavStage::EntryPath;
					EntryCheck.EntryIndex = EntryIndex;
					EntryCheck.bEntryToPlacement = true;
					if (!HBSpawnDiagnostics::ProjectNavigation(*World, Nav, Agent, NavData, Entry,
						FVector(100, 100, 200), -1., -1., nullptr, false, EntryOnNav, EntryCheck)) continue;
					FPathFindingQuery PathQuery(this, *NavData, EntryOnNav.Location, ApproachOnNav.Location);
					PathQuery.SetAllowPartialPaths(false).SetNavAgentProperties(*Agent);
					const FPathFindingResult Path = Nav->FindPathSync(*Agent, PathQuery);
					const bool bPathSucceeded = Path.IsSuccessful();
					const bool bPathValid = Path.Path.IsValid();
					if (EntryCheck.SetPathResult(bPathSucceeded, bPathValid,
						bPathValid && Path.Path->IsPartial()))
					{
						bReachable = true;
						break;
					}
				}
				if (!bReachable)
				{
					Reason = TEXT("Player has no complete navigation path to the chest approach.");
					Evidence.Cause = EHBSpawnFailureReason::NoCompleteEntryPath;
					Evidence.Details = FString::Printf(TEXT("EntriesAvailable=%d EntriesChecked=%d Direction=EntryToPlacement"),
						EntryLocations.Num(), Evidence.NavigationChecks.Num() - 1);
					return false;
				}
			}
			FActorSpawnParameters Params;
			Params.Owner = &Setup;
			Params.OverrideLevel = &RoomLevel;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			FTransform PointTransform = Choice.Point->GetActorTransform();
			PointTransform.SetLocation(CandidateMarker);
			FTransform SpawnTransform;
			if (!HBChestSpawnGeometry::MakeGroundedTransform(Choice.ChestClass.Get(),
				PointTransform, Ground.Z, SpawnTransform, Reason))
			{
				Evidence.Cause = EHBSpawnFailureReason::InvalidConfiguration;
				return false;
			}
			OutChest = World->SpawnActor<ABoxActor>(Choice.ChestClass, SpawnTransform, Params);
			if (!IsValid(OutChest))
			{
				Reason = FString::Printf(TEXT("Box %s at %s was blocked after floor alignment (floor Z=%.1f)."),
					*GetNameSafe(Choice.ChestClass.Get()), *SpawnTransform.GetLocation().ToString(), Ground.Z);
				Evidence.Cause = EHBSpawnFailureReason::SpawnCollision;
				return false;
			}
			return true;
		}, Floor, OutReason, PlacementReport)) return false;
	if (FVector::Dist2D(Marker, Floor.ImpactPoint) > KINDA_SMALL_NUMBER)
		UE_LOG(LogHBChestSpawn, Display, TEXT("Room %d point %s floor adjusted %.1f cm: marker=%s ground=%s actor=%s component=%s."),
			State.RoomId, *Choice.PointId.ToString(), FVector::Dist2D(Marker, Floor.ImpactPoint),
			*Marker.ToString(), *Floor.ImpactPoint.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()));
	if (!OutChest->GetIsReplicated() || OutChest->bOnlyRelevantToOwner || OutChest->GetLevel() != &RoomLevel)
	{
		OutChest->Destroy();
		OutChest = nullptr;
		OutReason = FString::Printf(TEXT("Spawned box %s must replicate to other players and belong to its source room level."),
			*GetNameSafe(Choice.ChestClass.Get()));
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidSpawnedActor, OutReason);
		return false;
	}
	return true;
}
