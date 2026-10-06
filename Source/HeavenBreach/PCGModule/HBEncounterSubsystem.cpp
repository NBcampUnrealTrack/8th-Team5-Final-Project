#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBSpawnGroundQuery.h"
#include "PCGModule/HBSpawnDiagnostics.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "AIController.h"
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

bool HBEncounterPlacement::AreCapsulesTooClose(const FVector& Location, float Radius, float HalfHeight,
	const FVector& OtherLocation, float OtherRadius, float OtherHalfHeight, float MinSpacing)
{
	if (FMath::Abs(Location.Z - OtherLocation.Z) >= HalfHeight + OtherHalfHeight) return false;
	return FVector::Dist2D(Location, OtherLocation) < Radius + OtherRadius + MinSpacing;
}

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
		FRandomStream& Random, TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&, FHBSpawnPlacementReport&)> TryPlace,
		FHBEncounterSpawnChoice& OutChoice, FHBSpawnSelectionReport& Report)
	{
		TArray<FHBEncounterMonsterOption> Remaining;
		for (const FHBEncounterMonsterOption& Option : Group.Monsters)
			if (Option.EnemyClass && Option.Weight > 0 && AllowsClass(Point, Option.EnemyClass.Get()))
				Remaining.Add(Option);
		if (Remaining.IsEmpty())
			Report.RecordSkip(Point.PointId, Group.GroupId, EHBSpawnFailureReason::NoCompatibleClass);
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
				Remaining[Pick].EnemyClass, Group.Grade, Group.bRequireEntryPath};
			Remaining.RemoveAt(Pick);
			FString Reason;
			FHBSpawnPlacementReport PlacementReport;
			if (TryPlace(Choice, Reason, PlacementReport))
			{
				Report.RecordPlaced(Point.PointId, Group.GroupId, Choice.EnemyClass->GetPathName(), &PlacementReport);
				OutChoice = Choice;
				return true;
			}
			Report.RecordClassFailure(Point.PointId, Group.GroupId, Choice.EnemyClass->GetPathName(), Reason, &PlacementReport);
		}
		return false;
	}
}

bool HBEncounterSelection::SelectAndPlace(const UHBEncounterComponent& Config,
	const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
	TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport)
{
	return SelectAndPlaceWithDiagnostics(Config, Points, DungeonSeed, RoomId,
		[&](const FHBEncounterSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport&)
		{ return TryPlace(Choice, Reason); }, OutPlaced, OutError, OutReport);
}

bool HBEncounterSelection::SelectAndPlaceWithDiagnostics(const UHBEncounterComponent& Config,
	const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
	TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&, FHBSpawnPlacementReport&)> TryPlace,
	TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport)
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
			Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
			return false;
		}
		AllIds.Add(Point->PointId);
		Report.AddPoint(Point->PointId, Point->bRequired);
	}
	FRandomStream Random(MakeRoomSeed(DungeonSeed, RoomId));
	TSet<FName> UsedIds;
	TMap<FName, int32> PlacedByGroup;
	for (EHBEncounterGrade Grade : {EHBEncounterGrade::Boss, EHBEncounterGrade::Elite, EHBEncounterGrade::Common})
	{
		for (const FHBEncounterGroup& Group : Config.Groups)
		{
			if (Group.Grade != Grade) continue;
			int32 Target = 0;
			const bool bConditionSpawned = !Group.ConditionGroupId.IsNone() && PlacedByGroup.FindRef(Group.ConditionGroupId) > 0;
			if (bConditionSpawned)
			{
				if (!HBSpawnCountSelection::Select(Group.MinCount, Group.MaxCount, EHBSpawnCountSelectionMode::Weighted,
					Group.CountWeightsIfConditionSpawned, Random, Target, OutError))
				{
					Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
					return false;
				}
			}
			else if (!HBSpawnCountSelection::Select(Group.MinCount, Group.MaxCount, Group.CountSelectionMode,
				Group.CountWeights, Random, Target, OutError))
			{
				Report.FailureReason = EHBSpawnFailureReason::InvalidConfiguration;
				return false;
			}
			FHBSpawnGroupReport& GroupReport = Report.Groups.AddDefaulted_GetRef();
			GroupReport.GroupId = Group.GroupId;
			GroupReport.MinCount = Group.MinCount;
			GroupReport.Target = Target;
			TArray<AHBEnemySpawnPoint*> Required;
			TArray<AHBEnemySpawnPoint*> Optional;
			for (AHBEnemySpawnPoint* Point : SortedPoints)
			{
				if (UsedIds.Contains(Point->PointId)) continue;
				if (!AllowsGrade(*Point, Grade))
				{
					Report.RecordSkip(Point->PointId, Group.GroupId, EHBSpawnFailureReason::GradeMismatch);
					continue;
				}
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
				AHBEnemySpawnPoint* Point = Required[PointIndex];
				FHBEncounterSpawnChoice Choice;
				if (TryOptionsAtPoint(*Point, Group, Random, TryPlace, Choice, Report))
				{
					OutPlaced.Add(Choice);
					UsedIds.Add(Choice.PointId);
					Report.RecordPlaced(Choice.PointId);
					++Placed;
				}
			}
			PlacedByGroup.Add(Group.GroupId, Placed);
			GroupReport.Placed = Placed;
			if (Placed < Group.MinCount)
			{
				Report.FailureReason = GroupReport.FailureReason = EHBSpawnFailureReason::MinCountNotMet;
				Report.FailedGroupId = Group.GroupId;
				OutError = FString::Printf(TEXT("Room %d group %s placed %d of required %d. FailureReason=MinCountNotMet Target=%d Seed=%d %s"),
					RoomId, *Group.GroupId.ToString(), Placed, Group.MinCount, Target, DungeonSeed,
					*Report.FormatGroupResults(Group.GroupId));
				return false;
			}
			if (Placed < Target)
				UE_LOG(LogHBEncounterSubsystem, Warning, TEXT("Room %d group %s placed %d of selected %d. %s"),
					RoomId, *Group.GroupId.ToString(), Placed, Target, *Report.FormatGroupResults(Group.GroupId));
		}
	}
	for (const AHBEnemySpawnPoint* Point : SortedPoints)
		if (Point->bRequired && !UsedIds.Contains(Point->PointId))
		{
			Report.FailureReason = EHBSpawnFailureReason::RequiredPointUnfilled;
			Report.FailedPointId = Point->PointId;
			OutError = Report.FormatRequiredFailure(Point->PointId, false);
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
	ULevel* RoomLevel, const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError, bool bDeferActivation)
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
	State.bDeferActivation = bDeferActivation;
	TArray<FHBEncounterSpawnChoice> Placed;
	const bool bSucceeded = HBEncounterSelection::SelectAndPlaceWithDiagnostics(*Setup->Encounter, Points, DungeonSeed, RoomId,
		[&](const FHBEncounterSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport& PlacementReport)
		{
			AEnemyCharacterBase* Enemy = nullptr;
			if (!TrySpawn(Choice, *Setup->Encounter, *Setup, *RoomLevel, RoomBounds, EntryLocations, State, Enemy, Reason, PlacementReport)) return false;
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
	State.bActivated = !bDeferActivation;
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

bool UHBEncounterSubsystem::IsActivatedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const
{
	if (!IsPreparedFor(GenerationId, RoomId, RoomLevel)) return false;
	for (const FRoomState& State : RoomStates)
		if (State.GenerationId == GenerationId && State.RoomId == RoomId && State.Level.Get() == RoomLevel)
			return State.bActivated;
	return false;
}

bool UHBEncounterSubsystem::ActivateGeneration(int32 GenerationId, FString& OutError)
{
	OutError.Reset();
	if (!CanManageRoomSpawns() || GenerationId <= 0)
	{
		OutError = TEXT("Encounter activation requires the server and a positive generation ID.");
		return false;
	}
	bool bFound = false;
	// Validate the entire generation before giving any pending enemy a controller.
	for (const FRoomState& State : RoomStates)
	{
		if (State.GenerationId != GenerationId) continue;
		bFound = true;
		if (!State.bPrepared || !State.Level.IsValid())
		{ OutError = TEXT("Encounter activation requires every room to be prepared and loaded."); return false; }
		if (State.bActivated) continue;
		for (const auto& EnemyPtr : State.SpawnedEnemies)
		{
			auto* Enemy = EnemyPtr.Get();
			if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || Enemy->GetLevel() != State.Level.Get())
			{ OutError = TEXT("A pending encounter enemy is missing or no longer belongs to its room."); return false; }
			if (Enemy->IsDead()) continue;
			if (Enemy->GetController() || !Enemy->AIControllerClass || !Enemy->GetEnemyASC())
			{ OutError = TEXT("A pending enemy must have an AI controller class and ASC, without early possession."); return false; }
		}
	}
	if (!bFound) { OutError = TEXT("No prepared encounters belong to this generation."); return false; }
	for (FRoomState& State : RoomStates)
	{
		if (State.GenerationId != GenerationId || State.bActivated) continue;
		for (const auto& EnemyPtr : State.SpawnedEnemies)
		{
			auto* Enemy = EnemyPtr.Get();
			if (Enemy->IsDead()) continue;
			Enemy->SpawnDefaultController();
			if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !Cast<AAIController>(Enemy->GetController())
				|| !Enemy->GetEnemyASC() || Enemy->GetEnemyASC()->GetAvatarActor() != Enemy)
			{
				OutError = FString::Printf(TEXT("Room %d enemy %s could not initialize its AI controller and ASC."),
					State.RoomId, *GetNameSafe(Enemy));
				return false;
			}
			Enemy->ForceNetUpdate();
		}
		State.bActivated = true;
	}
	return true;
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
	AEnemyCharacterBase*& OutEnemy, FString& OutReason, FHBSpawnPlacementReport& PlacementReport)
{
	OutEnemy = nullptr;
	UWorld* World = GetWorld();
	const auto* Defaults = Choice.EnemyClass ? Choice.EnemyClass->GetDefaultObject<AEnemyCharacterBase>() : nullptr;
	const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	if (!CanManageRoomSpawns() || !IsValid(Choice.Point) || !Capsule || Choice.Point->GetLevel() != &RoomLevel)
	{
		OutReason = TEXT("Missing world, room-owned point or enemy capsule.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidConfiguration, OutReason);
		return false;
	}
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if ((Choice.Point->MaxCapsuleRadius > 0 && Radius > Choice.Point->MaxCapsuleRadius)
		|| (Choice.Point->MaxCapsuleHalfHeight > 0 && HalfHeight > Choice.Point->MaxCapsuleHalfHeight))
	{
		OutReason = TEXT("Enemy capsule exceeds the point's size limit.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::CapsuleSizeExceeded, OutReason);
		return false;
	}
	const FVector Marker = Choice.Point->GetActorLocation();
	FHBSpawnGroundSearchSettings SearchSettings;
	SearchSettings.bAllowPositionCorrection = Choice.Point->bAllowPositionCorrection;
	SearchSettings.Basis = Choice.Point->bUseCapsuleBasedCorrection
		? EHBSpawnCorrectionBasis::EnemyCapsule : EHBSpawnCorrectionBasis::LegacyFixed;
	SearchSettings.ShapeRadius = Radius;
	SearchSettings.ShapeHalfExtents = FVector(Radius, Radius, HalfHeight);
	SearchSettings.RadiusMultiplier = Choice.Point->CorrectionRadiusMultiplier;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBEnemyFloor), false);
	Query.AddIgnoredActor(Choice.Point);
	FCollisionQueryParams FloorQuery = Query;
	for (const TWeakObjectPtr<AEnemyCharacterBase>& Other : State.SpawnedEnemies)
		if (IsValid(Other.Get())) FloorQuery.AddIgnoredActor(Other.Get());
	FHitResult Floor;
	if (!HBSpawnGroundQuery::FindPlacementWithDiagnostics(*World, Marker, Choice.Point->GetActorQuat(), RoomBounds,
		Config.MinFloorUpDot, FloorQuery,
		[&](const FHitResult& CandidateFloor, FString& Reason, FHBSpawnCandidateReport& Evidence)
		{
			const FVector Ground = CandidateFloor.ImpactPoint;
			const FVector SpawnLocation = Ground + FVector(0, 0, HalfHeight + 2.f);
			if (!RoomBounds.IsInsideOrOn(SpawnLocation))
			{
				Reason = TEXT("Projected spawn is outside the room bounds.");
				Evidence.Cause = EHBSpawnFailureReason::OutsideRoom;
				return false;
			}
			for (const FVector& Entry : EntryLocations)
				if (FVector::Dist2D(Ground, Entry) < Config.MinEntryDistance + Radius)
				{
					Reason = TEXT("Spawn blocks a room entry or player start.");
					Evidence.Cause = EHBSpawnFailureReason::EntrySpacing;
					return false;
				}
			for (const TWeakObjectPtr<AEnemyCharacterBase>& OtherPtr : State.SpawnedEnemies)
				if (AEnemyCharacterBase* Other = OtherPtr.Get())
				{
					const UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent();
					const float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();
					const float OtherHalfHeight = OtherCapsule->GetScaledCapsuleHalfHeight();
					if (HBEncounterPlacement::AreCapsulesTooClose(SpawnLocation, Radius, HalfHeight,
						Other->GetActorLocation(), OtherRadius, OtherHalfHeight, Config.MinMonsterSpacing))
					{
						Reason = TEXT("Spawn is too close to another monster.");
						Evidence.Cause = EHBSpawnFailureReason::MonsterSpacing;
						return false;
					}
				}
			for (AActor* Actor : RoomLevel.Actors)
				if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion))
				{
					const FBox Box = Exclusion->Bounds->Bounds.GetBox().ExpandBy(FVector(Radius, Radius, HalfHeight));
					if (Box.IsInsideOrOn(SpawnLocation))
					{
						Reason = TEXT("Spawn overlaps an authored exclusion region.");
						Evidence.Cause = EHBSpawnFailureReason::ExcludedArea;
						return false;
					}
				}
			const int32 CapsuleCheckIndex = PlacementReport.CapsuleChecks++;
			Evidence.CapsuleCheckIndex = CapsuleCheckIndex;
			const FQuat CapsuleRotation = Choice.Point->GetActorQuat();
			const FName CapsuleProfile = Capsule->GetCollisionProfileName();
			const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
			if (World->OverlapBlockingTestByProfile(SpawnLocation, CapsuleRotation, CapsuleProfile, CapsuleShape, Query))
			{
				FHBSpawnCapsuleFailure& Failure = PlacementReport.CapsuleFailures.Add_GetRef(
					HBSpawnDiagnostics::CollectCapsuleFailure(*World, Marker, CandidateFloor, SpawnLocation,
						CapsuleRotation, CapsuleProfile, CapsuleShape, Query, CapsuleCheckIndex));
				Failure.CandidateIndex = Evidence.CandidateIndex;
				UE_LOG(LogHBEncounterSubsystem, Verbose, TEXT("GenerationId=%d Seed=%d Room=%d Point=%s Class=%s %s"),
					State.GenerationId, State.DungeonSeed, State.RoomId, *Choice.PointId.ToString(),
					*Choice.EnemyClass->GetPathName(), *Failure.Format());
				Reason = TEXT("Enemy capsule overlaps blocking geometry.");
				Evidence.Cause = EHBSpawnFailureReason::CapsuleOverlap;
				return false;
			}
			UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
			const UCharacterMovementComponent* Movement = Defaults->GetCharacterMovement();
			const FNavAgentProperties* Agent = Movement ? &Movement->GetNavAgentPropertiesRef() : nullptr;
			const ANavigationData* NavData = Nav && Agent ? Nav->GetNavDataForProps(*Agent, SpawnLocation) : nullptr;
			FNavLocation OnNav;
			FHBSpawnNavCheck& GroundCheck = Evidence.NavigationChecks.AddDefaulted_GetRef();
			if (!HBSpawnDiagnostics::ProjectNavigation(*World, Nav, Agent, NavData, Ground,
				FVector(Radius, Radius, HalfHeight), Config.MaxNavProjectionDistance, Config.MaxNavProjectionHeight,
				&RoomBounds, false, OnNav, GroundCheck))
			{
				Reason = TEXT("No nearby navigation surface for this enemy's agent.");
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
					if (!HBSpawnDiagnostics::ProjectNavigation(*World, Nav, Agent, NavData, Entry,
						FVector(Radius, Radius, HalfHeight), Config.MaxNavProjectionDistance * 2.f,
						Config.MaxNavProjectionHeight * 2.f, nullptr, false, EntryOnNav, EntryCheck)) continue;
					FPathFindingQuery PathQuery(this, *NavData, OnNav.Location, EntryOnNav.Location);
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
					Reason = TEXT("Enemy has no complete navigation path to a room entry.");
					Evidence.Cause = EHBSpawnFailureReason::NoCompleteEntryPath;
					Evidence.Details = FString::Printf(TEXT("EntriesAvailable=%d EntriesChecked=%d Direction=PlacementToEntry"),
						EntryLocations.Num(), Evidence.NavigationChecks.Num() - 1);
					return false;
				}
			}
			return true;
		}, Floor, OutReason, PlacementReport, SearchSettings)) return false;
	const FVector Ground = Floor.ImpactPoint;
	const FVector SpawnLocation = Ground + FVector(0, 0, HalfHeight + 2.f);
	if (FVector::Dist2D(Marker, Ground) > KINDA_SMALL_NUMBER)
		UE_LOG(LogHBEncounterSubsystem, Display, TEXT("Room %d point %s floor adjusted %.1f cm: marker=%s ground=%s actor=%s component=%s."),
			State.RoomId, *Choice.PointId.ToString(), FVector::Dist2D(Marker, Ground),
			*Marker.ToString(), *Ground.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()));
	FActorSpawnParameters Params;
	Params.Owner = &Setup;
	Params.OverrideLevel = &RoomLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	Params.bDeferConstruction = State.bDeferActivation;
	OutEnemy = World->SpawnActor<AEnemyCharacterBase>(Choice.EnemyClass, SpawnLocation, Choice.Point->GetActorRotation(), Params);
	if (!IsValid(OutEnemy))
	{
		OutReason = TEXT("SpawnActor was rejected by collision handling.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::SpawnCollision, OutReason);
		return false;
	}
	if (State.bDeferActivation)
	{
		// Disable auto-possession before PostInitializeComponents/BeginPlay. Pausing
		// after ordinary spawning would already have executed OnPossess and AI tasks.
		const EAutoPossessAI PreviousAutoPossess = OutEnemy->AutoPossessAI;
		OutEnemy->AutoPossessAI = EAutoPossessAI::Disabled;
		OutEnemy->FinishSpawning(FTransform(Choice.Point->GetActorRotation(), SpawnLocation));
		OutEnemy->AutoPossessAI = PreviousAutoPossess;
	}
	if (!IsValid(OutEnemy) || OutEnemy->IsActorBeingDestroyed())
	{
		OutReason = TEXT("Enemy was destroyed while completing its spawn.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidSpawnedActor, OutReason);
		OutEnemy = nullptr;
		return false;
	}
	if (!OutEnemy->GetIsReplicated() || !OutEnemy->IsReplicatingMovement() || OutEnemy->bOnlyRelevantToOwner
		|| OutEnemy->GetLevel() != &RoomLevel)
	{
		OutReason = FString::Printf(TEXT("Spawned enemy %s must replicate actor/movement to other players and belong to its source room level."),
			*GetNameSafe(Choice.EnemyClass.Get()));
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidSpawnedActor, OutReason);
		OutEnemy->Destroy();
		OutEnemy = nullptr;
		return false;
	}
	if (!State.bDeferActivation && !OutEnemy->GetController()) OutEnemy->SpawnDefaultController();
	if (!OutEnemy->GetEnemyASC() || OutEnemy->IsDead() || !OutEnemy->AIControllerClass
		|| (State.bDeferActivation ? OutEnemy->GetController() != nullptr : !Cast<AAIController>(OutEnemy->GetController())))
	{
		OutReason = State.bDeferActivation
			? TEXT("Pending enemy needs an AI controller class, ASC and living state; Blueprint must not possess it before match start.")
			: TEXT("Spawned enemy has no AI controller, ASC or living state.");
		PlacementReport.RecordFailure(EHBSpawnFailureReason::InvalidSpawnedActor, OutReason);
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
	State.bActivated = false;
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
