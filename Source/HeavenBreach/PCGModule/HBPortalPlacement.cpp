#include "PCGModule/HBPortalPlacement.h"

namespace HBPortalPlacement
{
	bool SelectPairs(int32 Seed, int32 PairCount, const TArray<FCandidate>& Candidates,
		TArray<FPair>& OutPairs, FString& Error)
	{
		OutPairs.Reset();
		Error.Reset();
		if (PairCount < 1)
		{
			Error = TEXT("Portal pair count must be at least 1.");
			return false;
		}

		TArray<FPointKey> Floors[2];
		TSet<FPointKey> Seen;
		for (const FCandidate& Candidate : Candidates)
		{
			if (Candidate.Key.RoomId < 0 || Candidate.Key.PointId.IsNone())
			{
				Error = TEXT("Portal candidates require a nonnegative RoomId and a nonempty PointId.");
				return false;
			}
			if (Candidate.Floor != 1 && Candidate.Floor != 2)
			{
				Error = FString::Printf(TEXT("Portal candidate Room=%d Point=%s has unsupported floor %d; expected 1 or 2."),
					Candidate.Key.RoomId, *Candidate.Key.PointId.ToString(), Candidate.Floor);
				return false;
			}
			if (Seen.Contains(Candidate.Key))
			{
				Error = FString::Printf(TEXT("Duplicate portal candidate Room=%d Point=%s."),
					Candidate.Key.RoomId, *Candidate.Key.PointId.ToString());
				return false;
			}
			Seen.Add(Candidate.Key);
			Floors[Candidate.Floor - 1].Add(Candidate.Key);
		}

		if (Floors[0].Num() < PairCount || Floors[1].Num() < PairCount)
		{
			Error = FString::Printf(TEXT("Insufficient portal candidates: requested %d pairs; Floor1=%d, Floor2=%d (each floor needs at least %d)."),
				PairCount, Floors[0].Num(), Floors[1].Num(), PairCount);
			return false;
		}

		// Stable lexical order avoids depending on actor discovery order or FName allocation indices.
		// Salt the seed so portal selection never consumes room, enemy or chest random streams.
		FRandomStream Random(static_cast<int32>(static_cast<uint32>(Seed) ^ 0x504F5254u));
		for (TArray<FPointKey>& Floor : Floors)
		{
			Floor.Sort([](const FPointKey& A, const FPointKey& B)
			{
				return A.RoomId != B.RoomId ? A.RoomId < B.RoomId : A.PointId.LexicalLess(B.PointId);
			});
			for (int32 Index = Floor.Num() - 1; Index > 0; --Index)
				Floor.Swap(Index, Random.RandRange(0, Index));
		}

		OutPairs.Reserve(PairCount);
		for (int32 Index = 0; Index < PairCount; ++Index)
			OutPairs.Add({Index, Floors[0][Index], Floors[1][Index]});
		return true;
	}
}
