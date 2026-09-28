#pragma once
#include "CoreMinimal.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "HBDungeonModuleCatalog.generated.h"

struct FHBDungeonPortalDefinition
{
    FName PortalId;
    FTransform TriggerTransform;
    FVector TriggerExtent = FVector::ZeroVector;
    FTransform ArrivalTransform;
};

/** Derived from the referenced map; roles are never entered again on the generator. */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBModuleCatalogEntry
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") FName ModuleId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") TSoftObjectPtr<UWorld> Map;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") EHBDungeonModuleKind Kind = EHBDungeonModuleKind::Room;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") EHBPrototypeRoomRole Role = EHBPrototypeRoomRole::Unassigned;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") EHBAllowedFloor AllowedFloor = EHBAllowedFloor::Any;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") FBox Bounds = FBox(ForceInit);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") FVector InspectionEntry = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Module") int32 ConnectorCount = 0;
    TArray<FHBDungeonConnectorDefinition> Connectors;
    TArray<FHBDungeonOccupancyDefinition> Occupancy;
    TArray<FHBDungeonPortalDefinition> Portals;
};

namespace HBDungeonModuleCatalog
{
    HEAVENBREACH_API bool Build(const TArray<TSoftObjectPtr<UWorld>>& Maps,
        TArray<FHBModuleCatalogEntry>& OutEntries, TArray<FString>& OutErrors);
    /** Atomic refresh. Loads source maps without changing the active editor level or saving them. */
    HEAVENBREACH_API bool Build(const TArray<TSoftObjectPtr<UWorld>>& RoomMaps,
        const TArray<TSoftObjectPtr<UWorld>>& CorridorMaps, TArray<FHBModuleCatalogEntry>& OutEntries, TArray<FString>& OutErrors);
    /** Logical grid preview only. Physical connector routing is a separate step. */
    HEAVENBREACH_API bool Generate(const TArray<FHBModuleCatalogEntry>& Catalog,
        const HBDungeonLayout::FSettings& Settings, TArray<FHBPrototypeRoom>& OutRooms, FString& OutError);
}
