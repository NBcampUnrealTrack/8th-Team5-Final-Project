#pragma once

#include "CoreMinimal.h"

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
		/** Authored point transform composed with the selected room instance; not a searched arrival. */
		FTransform SpawnTransform = FTransform::Identity;
	};

	/** One reciprocal pair, rather than two independent directed links. PairId is generation-local. */
	struct FPair
	{
		int32 PairId = INDEX_NONE;
		FPointKey Floor1;
		FPointKey Floor2;
	};

	/**
	 * Select exactly PairCount points without replacement on each floor and randomly pair them.
	 * Input order does not affect the result. Uses its own seeded stream; no shared RNG is consumed.
	 * Candidates must already pass placement safety checks. On failure OutPairs is empty.
	 */
	HEAVENBREACH_API bool SelectPairs(int32 Seed, int32 PairCount, const TArray<FCandidate>& Candidates,
		TArray<FPair>& OutPairs, FString& Error);
}
