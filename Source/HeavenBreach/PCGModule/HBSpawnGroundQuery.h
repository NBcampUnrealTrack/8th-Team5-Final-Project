#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FCollisionQueryParams;
struct FHitResult;

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
		FHitResult& OutFloor, FString& OutError);
}
