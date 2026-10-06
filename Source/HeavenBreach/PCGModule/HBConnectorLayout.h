#pragma once

#include "CoreMinimal.h"
#include "PCGModule/HBDungeonModuleCatalog.h"

/** Physical, room-to-room LD test layout; does not change the legacy dungeon design rules. */
namespace HBConnectorLayout
{
	struct FSettings
	{
		int32 Seed = 12345;
		int32 RoomCount = 6;
		int32 MaxSearchSteps = 20000;
		double MaxSearchSeconds = 2.0;
		bool bAllowModuleReuse = true;
		FTransform Origin = FTransform::Identity;
	};

	/** Numerical contact tolerance only, NOT permission for doorway geometry to overlap. */
	constexpr double ContactTolerance = 0.1; // cm
	constexpr double SnapTolerance = 0.01; // cm
	HEAVENBREACH_API bool ValidateCatalog(const TArray<FHBModuleCatalogEntry>& Catalog, FString& OutError);

	HEAVENBREACH_API bool Generate(const TArray<FHBModuleCatalogEntry>& Catalog, const FSettings& Settings,
		TArray<FHBPrototypeRoom>& OutRooms, FString& OutError);
	HEAVENBREACH_API bool Validate(const TArray<FHBModuleCatalogEntry>& Catalog,
		const TArray<FHBPrototypeRoom>& Rooms, FString& OutError);
	HEAVENBREACH_API FTransform Align(const FTransform& ParentConnectorWorld, const FTransform& ChildConnectorLocal);
	/** First overlapping pair in the existing box iteration order. Extents are half-sizes along the box axes. */
	struct FOccupancyOverlap
	{
		int32 BoxAIndex = INDEX_NONE, BoxBIndex = INDEX_NONE;
		FTransform BoxAWorld = FTransform::Identity, BoxBWorld = FTransform::Identity;
		FVector ExtentA = FVector::ZeroVector, ExtentB = FVector::ZeroVector;
	};
	HEAVENBREACH_API bool OccupancyOverlaps(const FHBModuleCatalogEntry& A, const FTransform& TransformA,
		const FHBModuleCatalogEntry& B, const FTransform& TransformB, FOccupancyOverlap* OutOverlap = nullptr);
}
