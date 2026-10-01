#pragma once
#include "CoreMinimal.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "HBDungeonSnapshot.generated.h"

class AHBDungeonDoorSeal;

/** World-scoped authoritative layout, retained for late subscribers. */
USTRUCT()
struct FHBDungeonSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 GenerationId = 0;
    UPROPERTY() int32 Seed = 0;
    UPROPERTY() TArray<FHBPrototypeRoom> Rooms;
    /** Local walls use the same server-selected asset and collision size in every world. */
    UPROPERTY() TSoftClassPtr<AHBDungeonDoorSeal> DoorSealClass;
    UPROPERTY() FVector DoorSealExtent = FVector::ZeroVector;
};
