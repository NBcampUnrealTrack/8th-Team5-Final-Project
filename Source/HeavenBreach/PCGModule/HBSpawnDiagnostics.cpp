#include "PCGModule/HBSpawnDiagnostics.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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
		}
		return TEXT("Unknown");
	}

	FString FormatPoint(const FHBSpawnPointReport& Point, const FName* OnlyGroup = nullptr)
	{
		TArray<FString> Results;
		for (const FHBSpawnClassFailure& Failure : Point.ClassFailures)
		{
			if (OnlyGroup && Failure.GroupId != *OnlyGroup) continue;
			const FString Blocking = Failure.Placement.FormatBlockingSummary();
			const FString BlockingSuffix = Blocking.IsEmpty() ? FString() : FString::Printf(TEXT(" Blocking={%s}"), *Blocking);
			Results.Add(FString::Printf(TEXT("Group=%s Class=%s Cause=%s Detail={%s}%s"),
				*Failure.GroupId.ToString(), *Failure.ClassPath, ReasonName(Failure.Cause), *Failure.Details,
				*BlockingSuffix));
		}
		for (const FHBSpawnPointSkip& Skip : Point.Skips)
		{
			if (OnlyGroup && Skip.GroupId != *OnlyGroup) continue;
			Results.Add(FString::Printf(TEXT("Group=%s NotAttempted=%s"),
				*Skip.GroupId.ToString(), ReasonName(Skip.Reason)));
		}
		if (Results.IsEmpty()) Results.Add(TEXT("NotAttempted=PointNotAttempted"));
		return FString::Printf(TEXT("Point=%s Required=%d Placed=%d Results=[%s]"),
			*Point.PointId.ToString(), Point.bRequired ? 1 : 0, Point.bPlaced ? 1 : 0,
			*FString::Join(Results, TEXT("; ")));
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
	return FString::Printf(TEXT("Cause=%s CapsuleCheckIndex=%d Marker=%s GroundLocation=%s SpawnLocation=%s Rotation=%s Radius=%.3f HalfHeight=%.3f Profile=%s GroundActor=%s GroundComponent=%s GroundActorPath=%s GroundComponentPath=%s OriginalBlocking=1 MultiBlocking=%d QueryMismatch=%d UnresolvedBlockingResults=%d BlockingTargets=[%s]"),
		ReasonName(Cause), CapsuleCheckIndex, *Marker.ToString(), *GroundLocation.ToString(), *SpawnLocation.ToString(),
		*Rotation.ToString(), Radius, HalfHeight, *Profile.ToString(), *GroundActor, *GroundComponent,
		*GroundActorPath, *GroundComponentPath, bMultiHasBlocking ? 1 : 0, HasQueryMismatch() ? 1 : 0,
		UnresolvedBlockingResults, *FString::Join(Targets, TEXT("; ")));
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
	if (Placement) Failure.Placement = *Placement;
}

void FHBSpawnSelectionReport::RecordPlaced(FName PointId)
{
	Points.FindChecked(PointId).bPlaced = true;
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
			|| Point.Skips.ContainsByPredicate([GroupId](const FHBSpawnPointSkip& Skip)
				{ return Skip.GroupId == GroupId; }))
			Results.Add(FormatPoint(Point, &GroupId));
	}
	return FString::Printf(TEXT("PointResults=[%s]"), *FString::Join(Results, TEXT(" | ")));
}
