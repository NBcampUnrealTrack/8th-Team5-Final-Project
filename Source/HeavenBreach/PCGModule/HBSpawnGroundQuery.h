#pragma once

#include "CoreMinimal.h"
#include "PCGModule/HBSpawnDiagnostics.h"

class UWorld;
struct FCollisionQueryParams;
struct FHitResult;
struct FHBSpawnCandidateReport;
struct FHBSpawnPlacementReport;

/** Caller supplies the selected class's physical dimensions; this controls horizontal candidates only. */
struct FHBSpawnGroundSearchSettings
{
	bool bAllowPositionCorrection = true;
	EHBSpawnCorrectionBasis Basis = EHBSpawnCorrectionBasis::LegacyFixed;
	double ShapeRadius = 0.;
	double RadiusMultiplier = 2.;
	FVector ShapeHalfExtents = FVector::ZeroVector;
};

namespace HBSpawnGroundQuery
{
	constexpr double MaxHorizontalAdjustment = 30.;

	/** Try the authored marker first, then fixed horizontal offsets without consuming random draws.
	 * Every candidate must have a physical floor inside the room and pass the caller's placement checks.
	 * Vertical trace limits remain 100 cm above and 200 cm below the authored marker.
	 */
	HEAVENBREACH_API bool FindPlacement(UWorld& World, const FVector& Marker, const FQuat& MarkerRotation,
		const FBox& RoomBounds, float MinFloorUpDot, const FCollisionQueryParams& Query,
		TFunctionRef<bool(const FHitResult&, FString&)> TryPlace,
		FHitResult& OutFloor, FString& OutError,
		const FHBSpawnGroundSearchSettings& Settings = FHBSpawnGroundSearchSettings());

	HEAVENBREACH_API bool FindPlacementWithDiagnostics(UWorld& World, const FVector& Marker, const FQuat& MarkerRotation,
		const FBox& RoomBounds, float MinFloorUpDot, const FCollisionQueryParams& Query,
		TFunctionRef<bool(const FHitResult&, FString&, FHBSpawnCandidateReport&)> TryPlace,
		FHitResult& OutFloor, FString& OutError, FHBSpawnPlacementReport& Report,
		const FHBSpawnGroundSearchSettings& Settings = FHBSpawnGroundSearchSettings());
}
