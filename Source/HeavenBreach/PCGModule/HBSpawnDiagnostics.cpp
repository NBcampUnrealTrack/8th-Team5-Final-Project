#include "PCGModule/HBSpawnDiagnostics.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationSystemTypes.h"

namespace
{
	const TCHAR* ReasonName(EHBSpawnFailureReason Reason)
	{
		switch (Reason)
		{
		case EHBSpawnFailureReason::None: return TEXT("None");
		case EHBSpawnFailureReason::PlacementRejected: return TEXT("PlacementRejected");
		case EHBSpawnFailureReason::NoCompatibleClass: return TEXT("NoCompatibleClass");
		case EHBSpawnFailureReason::GroupMismatch: return TEXT("GroupMismatch");
		case EHBSpawnFailureReason::GradeMismatch: return TEXT("GradeMismatch");
		case EHBSpawnFailureReason::TargetReached: return TEXT("TargetReached");
		case EHBSpawnFailureReason::PointNotAttempted: return TEXT("PointNotAttempted");
		case EHBSpawnFailureReason::RequiredPointUnfilled: return TEXT("RequiredPointUnfilled");
		case EHBSpawnFailureReason::MinCountNotMet: return TEXT("MinCountNotMet");
		case EHBSpawnFailureReason::InvalidConfiguration: return TEXT("InvalidConfiguration");
		case EHBSpawnFailureReason::CapsuleOverlap: return TEXT("CapsuleOverlap");
		case EHBSpawnFailureReason::NoFloor: return TEXT("NoFloor");
		case EHBSpawnFailureReason::GroundPenetration: return TEXT("GroundPenetration");
		case EHBSpawnFailureReason::SteepGround: return TEXT("SteepGround");
		case EHBSpawnFailureReason::OutsideRoom: return TEXT("OutsideRoom");
		case EHBSpawnFailureReason::ExcludedArea: return TEXT("ExcludedArea");
		case EHBSpawnFailureReason::EntrySpacing: return TEXT("EntrySpacing");
		case EHBSpawnFailureReason::MonsterSpacing: return TEXT("MonsterSpacing");
		case EHBSpawnFailureReason::ChestSpacing: return TEXT("ChestSpacing");
		case EHBSpawnFailureReason::CapsuleSizeExceeded: return TEXT("CapsuleSizeExceeded");
		case EHBSpawnFailureReason::NavProjectionFailed: return TEXT("NavProjectionFailed");
		case EHBSpawnFailureReason::NoCompleteEntryPath: return TEXT("NoCompleteEntryPath");
		case EHBSpawnFailureReason::SpawnCollision: return TEXT("SpawnCollision");
		case EHBSpawnFailureReason::InvalidSpawnedActor: return TEXT("InvalidSpawnedActor");
		}
		return TEXT("Unknown");
	}

	FString FormatReasonCounts(const TMap<EHBSpawnFailureReason, int32>& Counts)
	{
		TArray<FString> Entries;
		for (const auto& Pair : Counts)
			Entries.Add(FString::Printf(TEXT("%s:%d"), ReasonName(Pair.Key), Pair.Value));
		Entries.Sort();
		return FString::Join(Entries, TEXT(", "));
	}

	void AddCounts(TMap<EHBSpawnFailureReason, int32>& Into, const TMap<EHBSpawnFailureReason, int32>& From)
	{
		for (const auto& Pair : From) Into.FindOrAdd(Pair.Key) += Pair.Value;
	}

	const TCHAR* NavConditionName(EHBSpawnNavCondition Condition)
	{
		switch (Condition)
		{
		case EHBSpawnNavCondition::None: return TEXT("None");
		case EHBSpawnNavCondition::MissingSystem: return TEXT("MissingSystem");
		case EHBSpawnNavCondition::MissingAgent: return TEXT("MissingAgent");
		case EHBSpawnNavCondition::MissingNavData: return TEXT("MissingNavData");
		case EHBSpawnNavCondition::NavigationBuildingOrLocked: return TEXT("NavigationBuildingOrLocked");
		case EHBSpawnNavCondition::ProjectionQueryFailed: return TEXT("ProjectionQueryFailed");
		case EHBSpawnNavCondition::HorizontalDistanceExceeded: return TEXT("HorizontalDistanceExceeded");
		case EHBSpawnNavCondition::HeightExceeded: return TEXT("HeightExceeded");
		case EHBSpawnNavCondition::OutsideRoom: return TEXT("OutsideRoom");
		case EHBSpawnNavCondition::PathQueryFailed: return TEXT("PathQueryFailed");
		case EHBSpawnNavCondition::InvalidPath: return TEXT("InvalidPath");
		case EHBSpawnNavCondition::PartialPath: return TEXT("PartialPath");
		}
		return TEXT("Unknown");
	}

	FString FormatPoint(const FHBSpawnPointReport& Point, const FName* OnlyGroup = nullptr)
	{
		TArray<FString> Results;
		TMap<EHBSpawnFailureReason, int32> Counts;
		for (const FHBSpawnClassFailure& Failure : Point.ClassFailures)
		{
			if (OnlyGroup && Failure.GroupId != *OnlyGroup) continue;
			AddCounts(Counts, Failure.Placement.CountFailureReasons());
			const FString Placement = Failure.Placement.FormatFailureSummary();
			const FString PlacementSuffix = Placement.IsEmpty() ? FString() : FString::Printf(TEXT(" Placement={%s}"), *Placement);
			const FString Blocking = Failure.Placement.FormatBlockingSummary();
			const FString BlockingSuffix = Blocking.IsEmpty() ? FString() : FString::Printf(TEXT(" Blocking={%s}"), *Blocking);
			Results.Add(FString::Printf(TEXT("Group=%s Class=%s Cause=%s Detail={%s}%s%s"),
				*Failure.GroupId.ToString(), *Failure.ClassPath, ReasonName(Failure.Cause), *Failure.Details,
				*PlacementSuffix, *BlockingSuffix));
		}
		for (const FHBSpawnClassPlacement& Placed : Point.ClassPlacements)
		{
			if (OnlyGroup && Placed.GroupId != *OnlyGroup) continue;
			AddCounts(Counts, Placed.Placement.CountFailureReasons());
			Results.Add(FString::Printf(TEXT("Group=%s Class=%s Placed=1 Placement={%s}"),
				*Placed.GroupId.ToString(), *Placed.ClassPath, *Placed.Placement.FormatFailureSummary()));
		}
		for (const FHBSpawnPointSkip& Skip : Point.Skips)
		{
			if (OnlyGroup && Skip.GroupId != *OnlyGroup) continue;
			Results.Add(FString::Printf(TEXT("Group=%s NotAttempted=%s"),
				*Skip.GroupId.ToString(), ReasonName(Skip.Reason)));
		}
		if (Results.IsEmpty()) Results.Add(TEXT("NotAttempted=PointNotAttempted"));
		return FString::Printf(TEXT("Point=%s Required=%d Placed=%d FailureCounts=[%s] Results=[%s]"),
			*Point.PointId.ToString(), Point.bRequired ? 1 : 0, Point.bPlaced ? 1 : 0,
			*FormatReasonCounts(Counts), *FString::Join(Results, TEXT("; ")));
	}

	FString FormatCounts(const TMap<FString, int32>& Counts)
	{
		TArray<FString> Paths;
		Counts.GetKeys(Paths);
		Paths.Sort();
		TArray<FString> Entries;
		for (const FString& Path : Paths)
			Entries.Add(FString::Printf(TEXT("%s:%d"), *Path, Counts.FindChecked(Path)));
		return FString::Join(Entries, TEXT(", "));
	}
}

bool FHBSpawnNavCheck::SetPathResult(bool bSucceeded, bool bValid, bool bPartial)
{
	bPathQueried = true;
	bPathSucceeded = bSucceeded;
	bPathValid = bValid;
	bPartialPath = bPartial;
	Condition = !bSucceeded ? EHBSpawnNavCondition::PathQueryFailed
		: !bValid ? EHBSpawnNavCondition::InvalidPath
		: bPartial ? EHBSpawnNavCondition::PartialPath : EHBSpawnNavCondition::None;
	return Condition == EHBSpawnNavCondition::None;
}

FString FHBSpawnNavCheck::Format() const
{
	const TCHAR* StageName = Stage == EHBSpawnNavStage::EnemyGround ? TEXT("EnemyGround")
		: Stage == EHBSpawnNavStage::ChestApproach ? TEXT("ChestApproach") : TEXT("EntryPath");
	return FString::Printf(TEXT("Stage=%s Condition=%s EntryIndex=%d AgentRadius=%.3f AgentHeight=%.3f NavData=%s Input=%s Projected=%s Extent=%s ProjectionQueried=%d ProjectionSucceeded=%d HorizontalDistance=%.3f HeightDelta=%.3f MaxDistance=%.3f MaxHeight=%.3f PathQueried=%d PathSucceeded=%d PathValid=%d PartialPath=%d Direction=%s"),
		StageName, NavConditionName(Condition), EntryIndex, AgentRadius, AgentHeight, *NavDataPath,
		*InputLocation.ToString(), *ProjectedLocation.ToString(), *ProjectionExtent.ToString(),
		bProjectionQueried ? 1 : 0, bProjectionSucceeded ? 1 : 0,
		bProjectionSucceeded ? FVector::Dist2D(InputLocation, ProjectedLocation) : -1.,
		bProjectionSucceeded ? FMath::Abs(InputLocation.Z - ProjectedLocation.Z) : -1., MaxHorizontalDistance, MaxHeight,
		bPathQueried ? 1 : 0, bPathSucceeded ? 1 : 0, bPathValid ? 1 : 0, bPartialPath ? 1 : 0,
		bEntryToPlacement ? TEXT("EntryToPlacement") : TEXT("PlacementToEntry"));
}

bool HBSpawnDiagnostics::ProjectNavigation(UWorld& World, UNavigationSystemV1* Nav,
	const FNavAgentProperties* Agent, const ANavigationData* NavData,
	const FVector& Input, const FVector& Extent, double MaxDistance, double MaxHeight,
	const FBox* RoomBounds, bool bRejectBuilding, FNavLocation& OutLocation, FHBSpawnNavCheck& Check)
{
	const EHBSpawnNavStage Stage = Check.Stage;
	const int32 EntryIndex = Check.EntryIndex;
	const bool bEntryToPlacement = Check.bEntryToPlacement;
	Check = FHBSpawnNavCheck();
	Check.Stage = Stage;
	Check.EntryIndex = EntryIndex;
	Check.bEntryToPlacement = bEntryToPlacement;
	Check.InputLocation = Input;
	Check.ProjectionExtent = Extent;
	Check.MaxHorizontalDistance = MaxDistance;
	Check.MaxHeight = MaxHeight;
	if (Agent) { Check.AgentRadius = Agent->AgentRadius; Check.AgentHeight = Agent->AgentHeight; }
	Check.NavDataPath = GetPathNameSafe(NavData);
	if (!Nav) Check.Condition = EHBSpawnNavCondition::MissingSystem;
	else if (!Agent) Check.Condition = EHBSpawnNavCondition::MissingAgent;
	else if (!NavData) Check.Condition = EHBSpawnNavCondition::MissingNavData;
	else if (bRejectBuilding && UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(&World))
		Check.Condition = EHBSpawnNavCondition::NavigationBuildingOrLocked;
	else
	{
		Check.bProjectionQueried = true;
		Check.bProjectionSucceeded = Nav->ProjectPointToNavigation(Input, OutLocation, Extent, NavData);
		if (Check.bProjectionSucceeded) Check.ProjectedLocation = OutLocation.Location;
		if (!Check.bProjectionSucceeded) Check.Condition = EHBSpawnNavCondition::ProjectionQueryFailed;
		else if (MaxDistance >= 0. && FVector::Dist2D(Input, OutLocation.Location) > MaxDistance)
			Check.Condition = EHBSpawnNavCondition::HorizontalDistanceExceeded;
		else if (MaxHeight >= 0. && FMath::Abs(Input.Z - OutLocation.Location.Z) > MaxHeight)
			Check.Condition = EHBSpawnNavCondition::HeightExceeded;
		else if (RoomBounds && !RoomBounds->IsInsideOrOn(OutLocation.Location))
			Check.Condition = EHBSpawnNavCondition::OutsideRoom;
	}
	return Check.Condition == EHBSpawnNavCondition::None;
}

FString FHBSpawnCandidateReport::Format() const
{
	TArray<FString> Nav;
	for (const FHBSpawnNavCheck& Check : NavigationChecks) Nav.Add(Check.Format());
	return FString::Printf(TEXT("CandidateIndex=%d Candidate=%s Accepted=%d Cause=%s HasFloor=%d Ground=%s Normal=%s GroundActorPath=%s GroundComponentPath=%s CapsuleCheckIndex=%d Detail={%s} Navigation=[%s]"),
		CandidateIndex, *CandidateLocation.ToString(), bAccepted ? 1 : 0, ReasonName(Cause), bHasFloor ? 1 : 0,
		*GroundLocation.ToString(), *GroundNormal.ToString(), *GroundActorPath, *GroundComponentPath,
		CapsuleCheckIndex, *Details, *FString::Join(Nav, TEXT("; ")));
}

void FHBSpawnPlacementReport::RecordFailure(EHBSpawnFailureReason Reason, const FString& Details)
{
	FailureReason = Reason;
	FailureDetails = Details;
}

TMap<EHBSpawnFailureReason, int32> FHBSpawnPlacementReport::CountFailureReasons() const
{
	TMap<EHBSpawnFailureReason, int32> Counts;
	for (const FHBSpawnCandidateReport& Candidate : Candidates)
		if (!Candidate.bAccepted && Candidate.Cause != EHBSpawnFailureReason::None) ++Counts.FindOrAdd(Candidate.Cause);
	// The search-level PlacementRejected is an aggregate, never an additional candidate rejection.
	if (FailureReason != EHBSpawnFailureReason::None && FailureReason != EHBSpawnFailureReason::PlacementRejected)
		++Counts.FindOrAdd(FailureReason);
	return Counts;
}

FString FHBSpawnPlacementReport::FormatFailureSummary() const
{
	if (Candidates.IsEmpty() && FailureReason == EHBSpawnFailureReason::None) return FString();
	TArray<FString> Causes, Examples;
	TSet<EHBSpawnFailureReason> SeenCauses;
	for (const FHBSpawnCandidateReport& Candidate : Candidates)
	{
		Causes.Add(FString::Printf(TEXT("%d:%s"), Candidate.CandidateIndex, ReasonName(Candidate.Cause)));
		if (!Candidate.bAccepted && !SeenCauses.Contains(Candidate.Cause))
		{
			SeenCauses.Add(Candidate.Cause);
			Examples.Add(Candidate.Format());
		}
	}
	// Keep every detailed candidate in the typed report; logs show exact indices/counts and representative queries.
	return FString::Printf(TEXT("FailureReason=%s CandidatesChecked=%d AcceptedCandidateIndex=%d FailureCounts=[%s] TerminalDetail={%s} CandidateCauses=[%s] FirstFailureByCause=[%s] LastCandidate={%s}"),
		ReasonName(FailureReason), Candidates.Num(), AcceptedCandidateIndex, *FormatReasonCounts(CountFailureReasons()),
		FailureReason == EHBSpawnFailureReason::PlacementRejected ? TEXT("") : *FailureDetails,
		*FString::Join(Causes, TEXT(", ")), *FString::Join(Examples, TEXT("; ")),
		Candidates.IsEmpty() ? TEXT("") : *Candidates.Last().Format());
}

void FHBSpawnCapsuleFailure::SetOverlaps(bool bHasBlocking, const TArray<FOverlapResult>& Overlaps)
{
	bMultiHasBlocking = bHasBlocking;
	BlockingTargets.Reset();
	UnresolvedBlockingResults = 0;
	TSet<FString> SeenComponents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!Overlap.bBlockingHit) continue;
		const AActor* Actor = Overlap.GetActor();
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsValid(Actor) || !IsValid(Component)) { ++UnresolvedBlockingResults; continue; }
		const FString ComponentPath = Component->GetPathName();
		if (SeenComponents.Contains(ComponentPath)) continue;
		SeenComponents.Add(ComponentPath);
		BlockingTargets.Add({Actor->GetName(), Actor->GetPathName(), Component->GetName(), ComponentPath});
	}
	BlockingTargets.Sort([](const FHBSpawnBlockingTarget& A, const FHBSpawnBlockingTarget& B)
	{
		return A.ActorPath == B.ActorPath ? A.ComponentPath < B.ComponentPath : A.ActorPath < B.ActorPath;
	});
}

bool FHBSpawnCapsuleFailure::HasQueryMismatch() const
{
	// The caller's original blocking test returned true. Missing resolved objects are reported separately.
	return !bMultiHasBlocking || (BlockingTargets.IsEmpty() && UnresolvedBlockingResults == 0);
}

FString FHBSpawnCapsuleFailure::Format() const
{
	TArray<FString> Targets;
	for (const FHBSpawnBlockingTarget& Target : BlockingTargets)
		Targets.Add(FString::Printf(TEXT("BlockingActor=%s BlockingComponent=%s ActorPath=%s ComponentPath=%s"),
			*Target.ActorName, *Target.ComponentName, *Target.ActorPath, *Target.ComponentPath));
	return FString::Printf(TEXT("Cause=%s CapsuleCheckIndex=%d Marker=%s GroundLocation=%s SpawnLocation=%s Rotation=%s Radius=%.3f HalfHeight=%.3f Profile=%s GroundActor=%s GroundComponent=%s GroundActorPath=%s GroundComponentPath=%s OriginalBlocking=1 MultiBlocking=%d QueryMismatch=%d UnresolvedBlockingResults=%d CandidateIndex=%d BlockingTargets=[%s]"),
		ReasonName(Cause), CapsuleCheckIndex, *Marker.ToString(), *GroundLocation.ToString(), *SpawnLocation.ToString(),
		*Rotation.ToString(), Radius, HalfHeight, *Profile.ToString(), *GroundActor, *GroundComponent,
		*GroundActorPath, *GroundComponentPath, bMultiHasBlocking ? 1 : 0, HasQueryMismatch() ? 1 : 0,
		UnresolvedBlockingResults, CandidateIndex, *FString::Join(Targets, TEXT("; ")));
}

FString FHBSpawnPlacementReport::FormatBlockingSummary() const
{
	if (CapsuleFailures.IsEmpty()) return FString();
	TMap<FString, int32> ActorCounts, ComponentCounts;
	int32 Mismatches = 0, Unresolved = 0;
	for (const FHBSpawnCapsuleFailure& Failure : CapsuleFailures)
	{
		Mismatches += Failure.HasQueryMismatch() ? 1 : 0;
		Unresolved += Failure.UnresolvedBlockingResults;
		TSet<FString> SeenActors, SeenComponents;
		for (const FHBSpawnBlockingTarget& Target : Failure.BlockingTargets)
		{
			if (!SeenActors.Contains(Target.ActorPath))
			{
				SeenActors.Add(Target.ActorPath);
				++ActorCounts.FindOrAdd(Target.ActorPath);
			}
			if (!SeenComponents.Contains(Target.ComponentPath))
			{
				SeenComponents.Add(Target.ComponentPath);
				++ComponentCounts.FindOrAdd(Target.ComponentPath);
			}
		}
	}
	return FString::Printf(TEXT("CapsuleChecks=%d CapsuleOverlapRejects=%d QueryMismatches=%d UnresolvedBlockingResults=%d CountsPerRejectedCheck=1 MultipleBlockersPerCheck=1 BlockingActorSummary=[%s] BlockingComponentSummary=[%s] LastCapsuleOverlap={%s}"),
		CapsuleChecks, CapsuleFailures.Num(), Mismatches, Unresolved, *FormatCounts(ActorCounts),
		*FormatCounts(ComponentCounts), *CapsuleFailures.Last().Format());
}

FHBSpawnCapsuleFailure HBSpawnDiagnostics::CollectCapsuleFailure(UWorld& World,
	const FVector& Marker, const FHitResult& Floor, const FVector& SpawnLocation, const FQuat& Rotation,
	FName Profile, const FCollisionShape& Shape, const FCollisionQueryParams& Query, int32 CapsuleCheckIndex)
{
	FHBSpawnCapsuleFailure Failure;
	Failure.CapsuleCheckIndex = CapsuleCheckIndex;
	Failure.Marker = Marker;
	Failure.GroundLocation = Floor.ImpactPoint;
	Failure.SpawnLocation = SpawnLocation;
	Failure.Rotation = Rotation;
	Failure.Radius = Shape.GetCapsuleRadius();
	Failure.HalfHeight = Shape.GetCapsuleHalfHeight();
	Failure.Profile = Profile;
	Failure.GroundActor = GetNameSafe(Floor.GetActor());
	Failure.GroundComponent = GetNameSafe(Floor.GetComponent());
	Failure.GroundActorPath = GetPathNameSafe(Floor.GetActor());
	Failure.GroundComponentPath = GetPathNameSafe(Floor.GetComponent());
	TArray<FOverlapResult> Overlaps;
	const bool bMultiHasBlocking = World.OverlapMultiByProfile(Overlaps, SpawnLocation, Rotation, Profile, Shape, Query);
	Failure.SetOverlaps(bMultiHasBlocking, Overlaps);
	return Failure;
}

void FHBSpawnSelectionReport::AddPoint(FName PointId, bool bRequired)
{
	FHBSpawnPointReport& Point = Points.FindOrAdd(PointId);
	Point.PointId = PointId;
	Point.bRequired = bRequired;
}

void FHBSpawnSelectionReport::RecordSkip(FName PointId, FName GroupId, EHBSpawnFailureReason Reason)
{
	Points.FindChecked(PointId).Skips.Add({GroupId, Reason});
}

void FHBSpawnSelectionReport::RecordClassFailure(FName PointId, FName GroupId,
	const FString& ClassPath, const FString& Details, const FHBSpawnPlacementReport* Placement)
{
	FHBSpawnClassFailure& Failure = Points.FindChecked(PointId).ClassFailures.AddDefaulted_GetRef();
	Failure.GroupId = GroupId;
	Failure.ClassPath = ClassPath;
	Failure.Details = Details.IsEmpty() ? FString(TEXT("Placement callback supplied no failure detail.")) : Details;
	if (Placement)
	{
		Failure.Placement = *Placement;
		if (Placement->FailureReason != EHBSpawnFailureReason::None) Failure.Cause = Placement->FailureReason;
	}
}

void FHBSpawnSelectionReport::RecordPlaced(FName PointId, FName GroupId, const FString& ClassPath,
	const FHBSpawnPlacementReport* Placement)
{
	FHBSpawnPointReport& Point = Points.FindChecked(PointId);
	Point.bPlaced = true;
	if (Placement) Point.ClassPlacements.Add({GroupId, ClassPath, *Placement});
}

TMap<EHBSpawnFailureReason, int32> FHBSpawnSelectionReport::CountFailureReasons(FName PointId, FName GroupId) const
{
	TMap<EHBSpawnFailureReason, int32> Counts;
	for (const auto& Pair : Points)
	{
		if (!PointId.IsNone() && Pair.Key != PointId) continue;
		for (const FHBSpawnClassFailure& Failure : Pair.Value.ClassFailures)
			if (GroupId.IsNone() || Failure.GroupId == GroupId) AddCounts(Counts, Failure.Placement.CountFailureReasons());
		for (const FHBSpawnClassPlacement& Placed : Pair.Value.ClassPlacements)
			if (GroupId.IsNone() || Placed.GroupId == GroupId) AddCounts(Counts, Placed.Placement.CountFailureReasons());
	}
	return Counts;
}

FString FHBSpawnSelectionReport::FormatRequiredFailure(FName PointId, bool bChest) const
{
	const FHBSpawnPointReport* Point = Points.Find(PointId);
	return FString::Printf(TEXT("Room %d required %spoint %s was not filled. FailureReason=RequiredPointUnfilled Seed=%d %s"),
		RoomId, bChest ? TEXT("chest ") : TEXT(""), *PointId.ToString(), DungeonSeed,
		Point ? *FormatPoint(*Point) : TEXT("NotAttempted=PointNotAttempted"));
}

FString FHBSpawnSelectionReport::FormatGroupResults(FName GroupId) const
{
	TArray<FName> PointIds;
	Points.GetKeys(PointIds);
	PointIds.Sort([](FName A, FName B) { return A.ToString() < B.ToString(); });
	TArray<FString> Results;
	for (FName PointId : PointIds)
	{
		const FHBSpawnPointReport& Point = Points.FindChecked(PointId);
		if (Point.ClassFailures.ContainsByPredicate([GroupId](const FHBSpawnClassFailure& Failure)
			{ return Failure.GroupId == GroupId; })
			|| Point.ClassPlacements.ContainsByPredicate([GroupId](const FHBSpawnClassPlacement& Placed)
				{ return Placed.GroupId == GroupId; })
			|| Point.Skips.ContainsByPredicate([GroupId](const FHBSpawnPointSkip& Skip)
				{ return Skip.GroupId == GroupId; }))
			Results.Add(FormatPoint(Point, &GroupId));
	}
	return FString::Printf(TEXT("FailureCounts=[%s] PointResults=[%s]"),
		*FormatReasonCounts(CountFailureReasons(NAME_None, GroupId)), *FString::Join(Results, TEXT(" | ")));
}
