#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class ULevel;
class AHBDungeonPortal;

/** Pure selection data; collection, placement safety and actor linking are handled by the runtime. */
namespace HBPortalPlacement
{
	/** PointId is source-room-local. RoomId distinguishes repeated generated instances. */
	struct FPointKey
	{
		int32 RoomId = INDEX_NONE;
		FName PointId;

		bool operator==(const FPointKey& Other) const { return RoomId == Other.RoomId && PointId == Other.PointId; }
		friend uint32 GetTypeHash(const FPointKey& Key) { return HashCombine(GetTypeHash(Key.RoomId), GetTypeHash(Key.PointId)); }
	};

	struct FCandidate
	{
		FPointKey Key;
		/** Logical room floor (1 or 2), never derived from world height. */
		int32 Floor = 0;
		/** Authored floor marker composed with the room instance. The portal origin is raised during preparation. */
		FTransform SpawnTransform = FTransform::Identity;
	};

	/** One reciprocal pair, rather than two independent directed links. PairId is generation-local. */
	struct FPair
	{
		int32 PairId = INDEX_NONE;
		FPointKey Floor1;
		FPointKey Floor2;
	};

	/** Conservative world-space reservations, derived from the actual trigger and player capsule. */
	struct FPlacement
	{
		FVector ActivationLocation = FVector::ZeroVector;
		FBox ActivationBounds = FBox(ForceInit);
		FBox ArrivalBounds = FBox(ForceInit);
		bool IsValid() const;
		bool Intersects(const FPlacement& Other) const;
	};

	struct FSpawnedPair
	{
		FPair Selection;
		TWeakObjectPtr<AHBDungeonPortal> Floor1;
		TWeakObjectPtr<AHBDungeonPortal> Floor2;
		FPlacement Placement1;
		FPlacement Placement2;
	};

	/**
	 * Select exactly PairCount points without replacement on each floor and randomly pair them.
	 * Input order does not affect the result. Uses its own seeded stream; no shared RNG is consumed.
	 * Candidates must already pass placement safety checks. On failure OutPairs is empty.
	 */
	HEAVENBREACH_API bool SelectPairs(int32 Seed, int32 PairCount, const TArray<FCandidate>& Candidates,
		TArray<FPair>& OutPairs, FString& Error);

	/** Server-only transaction. Tries spare points in stable shuffled order, never moves an authored floor marker.
	 * PreparePlacement grounds the constructed portal vertically and validates its physical space.
	 * Constructed actors use collision-free visuals and origin-based activation. Failure destroys every created actor.
	 */
	HEAVENBREACH_API bool SpawnPairs(UWorld& World, AActor* Owner, int32 Seed, int32 PairCount,
		TSubclassOf<AHBDungeonPortal> PortalClass, const TArray<FCandidate>& Candidates,
		TFunctionRef<ULevel*(int32)> ResolveLevel,
		TFunctionRef<bool(const FCandidate&, AHBDungeonPortal&, FPlacement&, FString&)> PreparePlacement,
		TArray<FSpawnedPair>& OutPairs, FString& Error);
	HEAVENBREACH_API void DestroyPairs(TArray<FSpawnedPair>& Pairs);
}
