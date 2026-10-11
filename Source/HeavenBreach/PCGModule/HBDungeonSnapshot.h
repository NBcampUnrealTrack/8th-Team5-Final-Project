#pragma once
#include "CoreMinimal.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "PCGModule/HBDungeonGenerationRules.h"
#include "HBDungeonSnapshot.generated.h"

class AHBDungeonDoorSeal;
class AHBDungeonPortal;

/** Final server placement. Clients copy it without selecting points or grounding again. */
USTRUCT()
struct FHBDungeonPortalEndpointSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 RoomId = INDEX_NONE;
    UPROPERTY() FName PointId;
    UPROPERTY() FTransform ActorTransform;
    UPROPERTY() FTransform TriggerTransform;
    UPROPERTY() FVector TriggerExtent = FVector::ZeroVector;
    UPROPERTY() FTransform ArrivalTransform;
    UPROPERTY() FVector ActivationLocation = FVector::ZeroVector;
    UPROPERTY() FBox ActivationBounds = FBox(ForceInit);
    UPROPERTY() FBox ArrivalBounds = FBox(ForceInit);
};

USTRUCT()
struct FHBDungeonPortalPairSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 PairId = INDEX_NONE;
    UPROPERTY() FHBDungeonPortalEndpointSnapshot Floor1;
    UPROPERTY() FHBDungeonPortalEndpointSnapshot Floor2;
};

UENUM()
enum class EHBDungeonSnapshotState : uint8 { Empty, Active, Cleared, Failed };

/** World-scoped authoritative layout, retained for late subscribers. */
USTRUCT()
struct FHBDungeonSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 GenerationId = 0;
    UPROPERTY() EHBDungeonSnapshotState State = EHBDungeonSnapshotState::Empty;
    UPROPERTY() FString FailureReason;
    UPROPERTY() int32 Seed = 0;
    UPROPERTY() TArray<FHBPrototypeRoom> Rooms;
    /** Local walls use the same server-selected asset and collision size in every world. */
    UPROPERTY() TSoftClassPtr<AHBDungeonDoorSeal> DoorSealClass;
    UPROPERTY() FVector DoorSealExtent = FVector::ZeroVector;
    UPROPERTY() EHBDungeonPortalPlacementMode PortalPlacementMode = EHBDungeonPortalPlacementMode::FixedRoomPortals;
    UPROPERTY() TSoftClassPtr<AHBDungeonPortal> PortalClass;
    UPROPERTY() int32 PortalPairCount = 0;
    /** Layout arrives first; clients wait for the same generation's final portal pairs. */
    UPROPERTY() bool bPortalPairsReady = false;
    UPROPERTY() TArray<FHBDungeonPortalPairSnapshot> PortalPairs;
};
