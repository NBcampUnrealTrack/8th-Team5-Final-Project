#pragma once

#include "CoreMinimal.h"
#include "PCGModule/HBRoomSpawnEntry.h"

class ULevel;

/** A path target is independent of the physical entries used for spawn spacing. */
struct FHBSpawnNavigationTarget
{
	FVector Location = FVector::ZeroVector;
	FName AnchorId;
	int32 EntryIndex = INDEX_NONE;
	bool bHasConnectionLocation = false;
	FVector ConnectionLocation = FVector::ZeroVector;
};

namespace HBSpawnNavigationTargets
{
	/** Collect world-space targets once per preparation, isolated to this streamed room instance. */
	HEAVENBREACH_API bool Collect(const ULevel& Level, const FBox& RoomBounds,
		TMap<FName, FVector>& OutAnchors, FString& OutError);
	/** Explicit anchors never fall back to room entries. Empty IDs preserve the existing entry policy. */
	HEAVENBREACH_API bool Resolve(FName AnchorId, const TMap<FName, FVector>& Anchors,
		const TArray<FHBRoomSpawnEntry>& Entries, TArray<FHBSpawnNavigationTarget>& OutTargets, FString& OutError);
}
