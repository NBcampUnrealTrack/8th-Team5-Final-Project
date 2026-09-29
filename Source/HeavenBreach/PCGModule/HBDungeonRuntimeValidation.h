#pragma once
#include "PCGModule/HBDungeonModuleCatalog.h"
class ULevelStreamingDynamic;
class AHBDungeonPortal;
namespace HBDungeonRuntimeValidation
{
    bool LinkPortals(const TArray<FHBPrototypeRoom>& Rooms, const TArray<FHBModuleCatalogEntry>& ModuleCatalog,
        const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
    bool ValidatePortalLinks(const TArray<FHBPrototypeRoom>& Rooms, const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
        const TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error);
}
