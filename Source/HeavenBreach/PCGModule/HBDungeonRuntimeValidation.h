#pragma once
#include "PCGModule/HBDungeonModuleCatalog.h"
class ULevelStreamingDynamic;
class AHBDungeonPortal;
class APlayerStart;
class ULevel;
namespace HBDungeonRuntimeValidation
{
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
