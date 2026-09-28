#pragma once
#include "CoreMinimal.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "HBDungeonSnapshot.generated.h"

/** World-scoped authoritative layout, retained for late subscribers. */
USTRUCT()
struct FHBDungeonSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 GenerationId = 0;
    UPROPERTY() int32 Seed = 0;
    UPROPERTY() TArray<FHBPrototypeRoom> Rooms;
};
