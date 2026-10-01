#pragma once
#include "PCGModule/HBDungeonModuleCatalog.h"
class ULevelStreamingDynamic;
class AHBDungeonPortal;
namespace HBDungeonRuntimeValidation
{
    /** Structural validation only; never runs planning or requires a source catalog. */
    bool ValidateLayoutForLoading(const TArray<FHBPrototypeRoom>& Rooms, FString& Error);
    /** Validate local portal actors without changing their authority-owned links. */
    bool ValidateLocalPortalInstances(const TArray<FHBPrototypeRoom>& Rooms,
        const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, FString& Error);
    bool LinkPortals(const TArray<FHBPrototypeRoom>& Rooms, const TArray<FHBModuleCatalogEntry>& ModuleCatalog,
        const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
    bool ValidatePortalLinks(const TArray<FHBPrototypeRoom>& Rooms, const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
        const TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
}
