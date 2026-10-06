#include "PCGModule/HBSpawnGroundQuery.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"

bool HBSpawnGroundQuery::FindPlacement(UWorld& World, const FVector& Marker, const FQuat& MarkerRotation,
	const FBox& RoomBounds, float MinFloorUpDot, const FCollisionQueryParams& Query,
	TFunctionRef<bool(const FHitResult&, FString&)> TryPlace,
	FHitResult& OutFloor, FString& OutError)
{
	OutFloor = FHitResult();
	OutError.Reset();
	if (!RoomBounds.IsValid || Marker.ContainsNaN() || MarkerRotation.ContainsNaN() || !FMath::IsFinite(MinFloorUpDot))
	{
		OutError = TEXT("Ground search needs valid room bounds, marker and floor slope limit.");
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
	for (const FVector& Candidate : Candidates)
	{
		FString Reason;
		FHitResult Floor;
		if (Candidate.X < RoomBounds.Min.X || Candidate.X > RoomBounds.Max.X
			|| Candidate.Y < RoomBounds.Min.Y || Candidate.Y > RoomBounds.Max.Y)
		{
			++OutsideRoom;
			Reason = FString::Printf(TEXT("Candidate %s is outside room bounds."), *Candidate.ToString());
		}
		else if (!World.LineTraceSingleByChannel(Floor, Candidate + FVector(0., 0., 100.),
			Candidate - FVector(0., 0., 200.), ECC_WorldStatic, Query))
		{
			++MissingFloor;
			Reason = FString::Printf(TEXT("No floor hit at %s (trace +100/-200 cm, WorldStatic)."), *Candidate.ToString());
		}
		else if (Floor.bStartPenetrating || Floor.ImpactNormal.Z < MinFloorUpDot)
		{
			++SteepFloor;
			Reason = FString::Printf(TEXT("Floor slope or penetration rejected at %s: actor=%s component=%s normal=%s minimum up=%.3f penetrating=%d."),
				*Candidate.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()),
				*Floor.ImpactNormal.ToString(), MinFloorUpDot, Floor.bStartPenetrating ? 1 : 0);
		}
		else if (!RoomBounds.IsInsideOrOn(Floor.ImpactPoint))
		{
			++OutsideRoom;
			Reason = FString::Printf(TEXT("Floor at %s is outside room bounds: actor=%s component=%s."),
				*Floor.ImpactPoint.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()));
		}
		else if (TryPlace(Floor, Reason))
		{
			OutFloor = Floor;
			return true;
		}
		else
		{
			++SafetyRejected;
			Reason = FString::Printf(TEXT("Placement rejected at %s: actor=%s component=%s; %s"),
				*Floor.ImpactPoint.ToString(), *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()), *Reason);
		}
		if (FirstReason.IsEmpty()) FirstReason = Reason;
		LastReason = MoveTemp(Reason);
	}
	OutError = FString::Printf(TEXT("%s Marker %s; %d candidates within %.0f cm: no floor=%d, steep/penetrating=%d, outside room=%d, safety rejected=%d. Original: %s Last: %s"),
		SafetyRejected == 0 ? TEXT("No safe floor or floor slope is too steep.") : TEXT("No safe placement near the spawn point."),
		*Marker.ToString(), Candidates.Num(), MaxHorizontalAdjustment, MissingFloor, SteepFloor, OutsideRoom, SafetyRejected,
		*FirstReason, *LastReason);
	return false;
}
