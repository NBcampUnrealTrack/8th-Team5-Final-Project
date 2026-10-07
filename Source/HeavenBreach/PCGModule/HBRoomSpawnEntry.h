#pragma once

#include "CoreMinimal.h"

/** World-space entry coordinates. Moving the navigation target never moves the connection or its spawn exclusion. */
struct FHBRoomSpawnEntry
{
	FVector ConnectionLocation;
	FVector NavigationLocation;

	explicit FHBRoomSpawnEntry(const FVector& Location)
		: ConnectionLocation(Location), NavigationLocation(Location) {}
	FHBRoomSpawnEntry(const FVector& Connection, const FVector& Navigation)
		: ConnectionLocation(Connection), NavigationLocation(Navigation) {}

	bool IsFinite() const { return !ConnectionLocation.ContainsNaN() && !NavigationLocation.ContainsNaN(); }
};

namespace HBRoomSpawnEntries
{
	/** Compatibility for callers that use one location for both entry checks. */
	inline TArray<FHBRoomSpawnEntry> FromLocations(const TArray<FVector>& Locations)
	{
		TArray<FHBRoomSpawnEntry> Entries;
		Entries.Reserve(Locations.Num());
		for (const FVector& Location : Locations) Entries.Emplace(Location);
		return Entries;
	}
}
