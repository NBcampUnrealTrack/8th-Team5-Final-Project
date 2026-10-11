#pragma once
#include "PCGModule/HBDungeonModuleCatalog.h"
#include "PCGModule/HBPortalPlacement.h"
class ULevelStreamingDynamic;
class AHBDungeonPortal;
class APlayerStart;
class ULevel;
class ACharacter;
struct FHBDungeonSnapshot;
struct FHBDungeonPortalPairSnapshot;
namespace HBDungeonRuntimeValidation
{
    bool ValidatePortalSnapshot(const FHBDungeonSnapshot& Snapshot, FString& Error);
    bool CapturePortalPairs(const TArray<HBPortalPlacement::FSpawnedPair>& Pairs,
        TArray<FHBDungeonPortalPairSnapshot>& OutPairs, FString& Error);
    /** Client-only materialization of final server data, without physics or navigation queries. */
    bool RestorePortalPairs(UWorld& World, AActor* Owner, TSubclassOf<AHBDungeonPortal> PortalClass,
        const FHBDungeonSnapshot& Snapshot, const TArray<ULevel*>& Levels,
        TArray<HBPortalPlacement::FSpawnedPair>& OutPairs, FString& Error);
	/** Compare loaded point actors with the catalog plan before any random portal is created. */
	bool ValidatePortalCandidates(const TArray<FHBPrototypeRoom>& Rooms, const TArray<ULevel*>& Levels,
		const TArray<HBPortalPlacement::FCandidate>& Candidates, FString& Error);
	/** Ground the trigger above the floor marker and arrival above its local floor, preserving XY/facing. No NavMesh queries. */
	bool PreparePortalSpace(UWorld& World, AHBDungeonPortal& Portal, const FVector& GroundPoint,
		const ACharacter& PlayerDefaults, const FBox& RoomBounds, HBPortalPlacement::FPlacement& OutPlacement, FString& Error);
	bool ValidatePortalSpace(UWorld& World, const AHBDungeonPortal& Portal, const ACharacter& PlayerDefaults,
		const FBox& RoomBounds, HBPortalPlacement::FPlacement& OutPlacement, FString& Error);
	bool ValidateRandomPortalLinks(const TArray<FHBPrototypeRoom>& Rooms, const TArray<ULevel*>& Levels,
		int32 PairCount, const TArray<HBPortalPlacement::FSpawnedPair>& Pairs, FString& Error);
    /** Structural validation only; never runs planning or requires a source catalog. */
    bool ValidateLayoutForLoading(const TArray<FHBPrototypeRoom>& Rooms, FString& Error);
    /** Local instance cleanup on server and clients; never modifies a source map or persistent level. */
    bool PreparePlayerStarts(const TArray<FHBPrototypeRoom>& Rooms, const TArray<ULevel*>& Levels,
        TArray<APlayerStart*>& OutStarts, FString& Error);
    /** Server-only choice from the prepared start room, using an independent deterministic seed stream. */
    APlayerStart* SelectPlayerStart(const TArray<APlayerStart*>& Starts, int32 DungeonSeed,
        TFunctionRef<bool(const APlayerStart&)> IsUsable, FString& Error);
    /** Validate local portal actors without changing their authority-owned links. */
    bool ValidateLocalPortalInstances(const TArray<FHBPrototypeRoom>& Rooms,
        const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, FString& Error);
    bool LinkPortals(const TArray<FHBPrototypeRoom>& Rooms, const TArray<FHBModuleCatalogEntry>& ModuleCatalog,
        const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
    bool ValidatePortalLinks(const TArray<FHBPrototypeRoom>& Rooms, const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
        const TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
}
