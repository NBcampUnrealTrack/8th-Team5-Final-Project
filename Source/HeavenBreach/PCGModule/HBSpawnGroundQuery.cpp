#include "PCGModule/HBSpawnGroundQuery.h"
#include "Engine/World.h"
#include "PCGModule/HBSpawnDiagnostics.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"

bool HBSpawnGroundQuery::FindPlacement(UWorld& World, const FVector& Marker, const FQuat& MarkerRotation,
	const FBox& RoomBounds, float MinFloorUpDot, const FCollisionQueryParams& Query,
	TFunctionRef<bool(const FHitResult&, FString&)> TryPlace,
	FHitResult& OutFloor, FString& OutError)
{
	FHBSpawnPlacementReport Report;
	return FindPlacementWithDiagnostics(World, Marker, MarkerRotation, RoomBounds, MinFloorUpDot, Query,
		[&](const FHitResult& Floor, FString& Reason, FHBSpawnCandidateReport&)
		{ return TryPlace(Floor, Reason); }, OutFloor, OutError, Report);
}

bool HBSpawnGroundQuery::FindPlacementWithDiagnostics(UWorld& World, const FVector& Marker, const FQuat& MarkerRotation,
	const FBox& RoomBounds, float MinFloorUpDot, const FCollisionQueryParams& Query,
	TFunctionRef<bool(const FHitResult&, FString&, FHBSpawnCandidateReport&)> TryPlace,
	FHitResult& OutFloor, FString& OutError, FHBSpawnPlacementReport& Report)
{
	Report = FHBSpawnPlacementReport();
	OutFloor = FHitResult();
	OutError.Reset();
	if (!RoomBounds.IsValid || Marker.ContainsNaN() || MarkerRotation.ContainsNaN() || !FMath::IsFinite(MinFloorUpDot))
	{
		OutError = TEXT("Ground search needs valid room bounds, marker and floor slope limit.");
		Report.RecordFailure(EHBSpawnFailureReason::InvalidConfiguration, OutError);
		return false;
	}

	// Only yaw is used: a tilted point must not move a candidate to a different floor.
	const FQuat Yaw = FRotator(0., MarkerRotation.Rotator().Yaw, 0.).Quaternion();
	constexpr double Diagonal = 0.7071067811865475;
	const FVector Directions[] = {
		FVector(1., 0., 0.), FVector(0., 1., 0.), FVector(-1., 0., 0.), FVector(0., -1., 0.),
		FVector(Diagonal, Diagonal, 0.), FVector(-Diagonal, Diagonal, 0.),
		FVector(-Diagonal, -Diagonal, 0.), FVector(Diagonal, -Diagonal, 0.)
	};
	TArray<FVector, TInlineAllocator<25>> Candidates;
	Candidates.Add(Marker);
	for (double Radius : {10., 20., MaxHorizontalAdjustment})
		for (const FVector& Direction : Directions)
			Candidates.Add(Marker + Yaw.RotateVector(Direction * Radius));

	int32 MissingFloor = 0, SteepFloor = 0, OutsideRoom = 0, SafetyRejected = 0;
	FString FirstReason, LastReason;
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const FVector& Candidate = Candidates[CandidateIndex];
		FHBSpawnCandidateReport& Evidence = Report.Candidates.AddDefaulted_GetRef();
		Evidence.CandidateIndex = CandidateIndex;
		Evidence.CandidateLocation = Candidate;
		FString Reason;
		FHitResult Floor;
		if (Candidate.X < RoomBounds.Min.X || Candidate.X > RoomBounds.Max.X
			|| Candidate.Y < RoomBounds.Min.Y || Candidate.Y > RoomBounds.Max.Y)
		{
			++OutsideRoom;
			Evidence.Cause = EHBSpawnFailureReason::OutsideRoom;
			Reason = FString::Printf(TEXT("Candidate %s is outside room bounds."), *Candidate.ToString());
		}
		else if (!World.LineTraceSingleByChannel(Floor, Candidate + FVector(0., 0., 100.),
			Candidate - FVector(0., 0., 200.), ECC_WorldStatic, Query))
		{
			++MissingFloor;
			Evidence.Cause = EHBSpawnFailureReason::NoFloor;
			Reason = FString::Printf(TEXT("No floor hit at %s (trace +100/-200 cm, WorldStatic)."), *Candidate.ToString());
		}
		else if (Floor.bStartPenetrating || Floor.ImpactNormal.Z < MinFloorUpDot)
		{
			++SteepFloor;
			Evidence.Cause = Floor.bStartPenetrating ? EHBSpawnFailureReason::GroundPenetration : EHBSpawnFailureReason::SteepGround;
			Reason = FString::Printf(TEXT("Floor slope or penetration rejected at %s: actor=%s component=%s normal=%s minimum up=%.3f penetrating=%d."),
				*Candidate.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()),
				*Floor.ImpactNormal.ToString(), MinFloorUpDot, Floor.bStartPenetrating ? 1 : 0);
		}
		else if (!RoomBounds.IsInsideOrOn(Floor.ImpactPoint))
		{
			++OutsideRoom;
			Evidence.Cause = EHBSpawnFailureReason::OutsideRoom;
			Reason = FString::Printf(TEXT("Floor at %s is outside room bounds: actor=%s component=%s."),
				*Floor.ImpactPoint.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()));
		}
		else if (TryPlace(Floor, Reason, Evidence))
		{
			Evidence.bHasFloor = true;
			Evidence.GroundLocation = Floor.ImpactPoint;
			Evidence.GroundNormal = Floor.ImpactNormal;
			Evidence.GroundActorPath = GetPathNameSafe(Floor.GetActor());
			Evidence.GroundComponentPath = GetPathNameSafe(Floor.GetComponent());
			Evidence.bAccepted = true;
			Evidence.Cause = EHBSpawnFailureReason::None;
			Report.AcceptedCandidateIndex = CandidateIndex;
			OutFloor = Floor;
			return true;
		}
		else
		{
			++SafetyRejected;
			if (Evidence.Cause == EHBSpawnFailureReason::None) Evidence.Cause = EHBSpawnFailureReason::PlacementRejected;
			Reason = FString::Printf(TEXT("Placement rejected at %s: actor=%s component=%s; %s"),
				*Floor.ImpactPoint.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()), *Reason);
		}
		Evidence.bHasFloor = Floor.bBlockingHit;
		if (Evidence.bHasFloor)
		{
			Evidence.GroundLocation = Floor.ImpactPoint;
			Evidence.GroundNormal = Floor.ImpactNormal;
			Evidence.GroundActorPath = GetPathNameSafe(Floor.GetActor());
			Evidence.GroundComponentPath = GetPathNameSafe(Floor.GetComponent());
		}
		Evidence.Details = Evidence.Details.IsEmpty() ? Reason : FString::Printf(TEXT("%s; %s"), *Reason, *Evidence.Details);
		if (FirstReason.IsEmpty()) FirstReason = Reason;
		LastReason = MoveTemp(Reason);
	}
	OutError = FString::Printf(TEXT("%s Marker %s; %d candidates within %.0f cm: no floor=%d, steep/penetrating=%d, outside room=%d, safety rejected=%d. Original: %s Last: %s"),
		SafetyRejected == 0 ? TEXT("No safe floor or floor slope is too steep.") : TEXT("No safe placement near the spawn point."),
		*Marker.ToString(), Candidates.Num(), MaxHorizontalAdjustment, MissingFloor, SteepFloor, OutsideRoom, SafetyRejected,
		*FirstReason, *LastReason);
	Report.RecordFailure(EHBSpawnFailureReason::PlacementRejected, OutError);
	return false;
}
