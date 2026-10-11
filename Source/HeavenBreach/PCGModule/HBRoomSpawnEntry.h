#pragma once

#include "CoreMinimal.h"

/** World-space entry coordinates. Moving the navigation target never moves the connection or its spawn exclusion. */
struct FHBRoomSpawnEntry
{
	FVector ConnectionLocation;
	FVector NavigationLocation;
	/** Portals reserve physical space but do not become NPC/chest path targets. Other entries default to true. */
	bool bUseForNavigation = true;
	/** Optional reserved portal spaces; independent of legacy entry distance/path policies. */
	TArray<FBox> ReservedBounds;

	explicit FHBRoomSpawnEntry(const FVector& Location)
		: ConnectionLocation(Location), NavigationLocation(Location) {}
	FHBRoomSpawnEntry(const FVector& Connection, const FVector& Navigation)
		: ConnectionLocation(Connection), NavigationLocation(Navigation) {}

	bool IsFinite() const
	{
		if (ConnectionLocation.ContainsNaN() || NavigationLocation.ContainsNaN()) return false;
		for (const FBox& Box : ReservedBounds)
			if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN()) return false;
		return true;
	}
	bool IntersectsReservation(const FBox& SpawnBounds) const
	{
		for (const FBox& Reserved : ReservedBounds)
			if (Reserved.Intersect(SpawnBounds)) return true;
		return false;
	}
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
