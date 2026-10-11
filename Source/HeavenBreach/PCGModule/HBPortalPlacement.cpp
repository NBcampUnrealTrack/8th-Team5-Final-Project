#include "PCGModule/HBPortalPlacement.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"

namespace HBPortalPlacement
{
	static bool ShuffleCandidates(int32 Seed, int32 PairCount, const TArray<FCandidate>& Candidates,
		TArray<FPointKey> (&Floors)[2], FString& Error)
	{
		Error.Reset();
		if (PairCount < 1)
		{
			Error = TEXT("Portal pair count must be at least 1.");
			return false;
		}

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

		return true;
	}

	bool SelectPairs(int32 Seed, int32 PairCount, const TArray<FCandidate>& Candidates,
		TArray<FPair>& OutPairs, FString& Error)
	{
		OutPairs.Reset();
		TArray<FPointKey> Floors[2];
		if (!ShuffleCandidates(Seed, PairCount, Candidates, Floors, Error)) return false;
		OutPairs.Reserve(PairCount);
		for (int32 Index = 0; Index < PairCount; ++Index)
			OutPairs.Add({Index, Floors[0][Index], Floors[1][Index]});
		return true;
	}

	bool FPlacement::IsValid() const
	{
		return !ActivationLocation.ContainsNaN() && ActivationBounds.IsValid && ArrivalBounds.IsValid
			&& !ActivationBounds.Min.ContainsNaN() && !ActivationBounds.Max.ContainsNaN()
			&& !ArrivalBounds.Min.ContainsNaN() && !ArrivalBounds.Max.ContainsNaN();
	}

	bool FPlacement::Intersects(const FPlacement& Other) const
	{
		return ActivationBounds.Intersect(Other.ActivationBounds) || ActivationBounds.Intersect(Other.ArrivalBounds)
			|| ArrivalBounds.Intersect(Other.ActivationBounds) || ArrivalBounds.Intersect(Other.ArrivalBounds);
	}

	void DestroyPairs(TArray<FSpawnedPair>& Pairs)
	{
		for (const FSpawnedPair& Pair : Pairs)
			for (AHBDungeonPortal* Portal : {Pair.Floor1.Get(), Pair.Floor2.Get()})
				if (IsValid(Portal)) { Portal->Disconnect(); Portal->Destroy(); }
		Pairs.Reset();
	}

	bool SpawnPairs(UWorld& World, AActor* Owner, int32 Seed, int32 PairCount,
		TSubclassOf<AHBDungeonPortal> PortalClass, const TArray<FCandidate>& Candidates,
		TFunctionRef<ULevel*(int32)> ResolveLevel,
		TFunctionRef<bool(const FCandidate&, AHBDungeonPortal&, FPlacement&, FString&)> PreparePlacement,
		TArray<FSpawnedPair>& OutPairs, FString& Error)
	{
		Error.Reset();
		if (!OutPairs.IsEmpty()) { Error = TEXT("Clear the previous portal pairs before spawning a generation."); return false; }
		if (World.GetNetMode() == NM_Client || !PortalClass || PortalClass->HasAnyClassFlags(CLASS_Abstract)
			|| PortalClass->GetDefaultObject<AHBDungeonPortal>()->GetIsReplicated())
		{ Error = TEXT("Random portals require a concrete, non-replicated portal class and the server world."); return false; }
		TArray<FPointKey> Floors[2];
		if (!ShuffleCandidates(Seed, PairCount, Candidates, Floors, Error)) return false;
		TMap<FPointKey, const FCandidate*> ByKey;
		for (const FCandidate& Candidate : Candidates)
		{
			ULevel* Level = ResolveLevel(Candidate.Key.RoomId);
			if (!IsValid(Level) || Level->GetWorld() != &World || !Candidate.SpawnTransform.IsValid()
				|| !Candidate.SpawnTransform.GetScale3D().Equals(FVector::OneVector, 0.0001))
			{ Error = TEXT("Random portal candidate has an invalid room level or spawn transform."); return false; }
			ByKey.Add(Candidate.Key, &Candidate);
		}

		TArray<FSpawnedPair> Prepared;
		TSet<FPointKey> Unavailable;
		FString LastRejection;
		int32 Attempts = 0;
		auto TryPoint = [&](const FPointKey& Key, FPlacement& Placement) -> AHBDungeonPortal*
		{
			const FCandidate& Candidate = *ByKey[Key];
			++Attempts;
			FActorSpawnParameters Params;
			Params.Owner = Owner;
			Params.OverrideLevel = ResolveLevel(Key.RoomId);
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.bDeferConstruction = true;
			AHBDungeonPortal* Portal = World.SpawnActor<AHBDungeonPortal>(PortalClass, Candidate.SpawnTransform, Params);
			FString Reason;
			if (IsValid(Portal))
			{
				Portal->PortalId = FName(*FString::Printf(TEXT("PCG_R%d_%s"), Key.RoomId, *Key.PointId.ToString()));
				Portal->SetTravelEnabled(false);
				Portal->SetActorEnableCollision(false);
				Portal->FinishSpawning(Candidate.SpawnTransform);
				if (IsValid(Portal) && !Portal->IsActorBeingDestroyed())
				{
					Portal->SetTravelEnabled(false);
					Portal->SetActorEnableCollision(false);
					Portal->PortalId = FName(*FString::Printf(TEXT("PCG_R%d_%s"), Key.RoomId, *Key.PointId.ToString()));
					// Activation is tested by pawn origin. Visual meshes must not block the entrance or alter navigation.
					TInlineComponentArray<UPrimitiveComponent*> Components(Portal);
					for (UPrimitiveComponent* Component : Components)
					{
						Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Component->SetCanEverAffectNavigation(false);
					}
					if (Portal->GetLevel() == Params.OverrideLevel && !Portal->GetIsReplicated()
						&& !IsValid(Portal->TargetPortal) && Portal->GetActorTransform().Equals(Candidate.SpawnTransform, 0.1)
						&& Portal->ValidateDefinition(Reason)
						&& PreparePlacement(Candidate, *Portal, Placement, Reason) && Placement.IsValid()
						&& Portal->ValidateDefinition(Reason))
						return Portal;
				}
				Portal->Destroy();
			}
			Unavailable.Add(Key);
			LastRejection = FString::Printf(TEXT("Room=%d Point=%s: %s"), Key.RoomId, *Key.PointId.ToString(),
				Reason.IsEmpty() ? TEXT("constructed portal or placement is invalid") : *Reason);
			return nullptr;
		};
		auto ConflictsWithPrepared = [&](const FPlacement& Placement)
		{
			for (const FSpawnedPair& Pair : Prepared)
				if (Placement.Intersects(Pair.Placement1) || Placement.Intersects(Pair.Placement2)) return true;
			return false;
		};

		for (int32 PairId = 0; PairId < PairCount; ++PairId)
		{
			bool bFoundPair = false;
			for (const FPointKey& Key1 : Floors[0])
			{
				if (Unavailable.Contains(Key1)) continue;
				FPlacement Placement1;
				AHBDungeonPortal* Portal1 = TryPoint(Key1, Placement1);
				if (!Portal1) continue;
				if (ConflictsWithPrepared(Placement1))
				{
					LastRejection = FString::Printf(TEXT("Room=%d Point=%s overlaps a reserved portal space."), Key1.RoomId, *Key1.PointId.ToString());
					Unavailable.Add(Key1); Portal1->Destroy(); continue;
				}
				for (const FPointKey& Key2 : Floors[1])
				{
					if (Unavailable.Contains(Key2)) continue;
					FPlacement Placement2;
					AHBDungeonPortal* Portal2 = TryPoint(Key2, Placement2);
					if (!Portal2) continue;
					if (ConflictsWithPrepared(Placement2) || Placement1.Intersects(Placement2))
					{
						LastRejection = FString::Printf(TEXT("Room=%d Point=%s overlaps a reserved portal space."), Key2.RoomId, *Key2.PointId.ToString());
						if (ConflictsWithPrepared(Placement2)) Unavailable.Add(Key2);
						Portal2->Destroy(); continue;
					}
					if (!Portal1->ConnectTo(Portal2))
					{ LastRejection = TEXT("Could not establish reciprocal portal links."); Portal2->Destroy(); continue; }
					Prepared.Add({{PairId, Key1, Key2}, Portal1, Portal2, Placement1, Placement2});
					Unavailable.Add(Key1); Unavailable.Add(Key2);
					bFoundPair = true;
					break;
				}
				if (bFoundPair) break;
				Portal1->Destroy();
			}
			if (!bFoundPair)
			{
				const int32 PreparedCount = Prepared.Num();
				DestroyPairs(Prepared);
				Error = FString::Printf(TEXT("Random portal placement failed: requested=%d prepared=%d Floor1=%d Floor2=%d attempts=%d; %s"),
					PairCount, PreparedCount, Floors[0].Num(), Floors[1].Num(), Attempts, *LastRejection);
				return false;
			}
		}
		OutPairs = MoveTemp(Prepared);
		return true;
	}
}
