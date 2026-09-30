#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "HBDungeonModuleDefinition.generated.h"

class UHBRoomInfoComponent;
class UHBDungeonConnectorComponent;
class UHBDungeonOccupancyComponent;
class ULevel;

UENUM(BlueprintType)
enum class EHBDungeonModuleKind : uint8 { Room, Corridor };

/** Read-only validation result; Source is also used by clickable Message Log tokens. */
struct HEAVENBREACH_API FHBDungeonModuleIssue
{
	FName Code;
	FString Message;
	TWeakObjectPtr<UObject> Source;
};

struct HEAVENBREACH_API FHBDungeonConnectorDefinition
{
	FName ConnectorId;
	FTransform LocalTransform;
	EHBDungeonConnectorRole Role = EHBDungeonConnectorRole::None;
};

struct HEAVENBREACH_API FHBDungeonOccupancyDefinition
{
	/** Position/rotation only; scale is baked into Extent exactly once. */
	FTransform LocalTransform;
	/** Effective half-size in cm, including the authored component/world scale. */
	FVector Extent = FVector::ZeroVector;
};

/** One per source module level. Existing geometry is deliberately not attached to this actor. */
UCLASS(Blueprintable, HideCategories=(Replication, Networking, Input, Collision, Physics, Actor, LOD))
class HEAVENBREACH_API AHBDungeonModuleDefinition : public AActor
{
	GENERATED_BODY()
public:
	AHBDungeonModuleDefinition();

	/** Shared identity for rooms AND corridors; unique across the future catalog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Module")
	FName ModuleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Module")
	EHBDungeonModuleKind ModuleKind = EHBDungeonModuleKind::Room;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|Module")
	TObjectPtr<UHBRoomInfoComponent> RoomInfo;

	/** Checks this actor's level only; does not save assets or run collision/placement tests. */
	UFUNCTION(CallInEditor, Category="Dungeon|Validation", meta=(DisplayName="Validate Module"))
	void ValidateModule();

	/** Reusable by a catalog builder, including the case of a level with no definition actor. */
	static bool ValidateLevel(ULevel* Level, TArray<FHBDungeonModuleIssue>& OutIssues);

	/**
	 * Read settings in the MODULE frame, never the definition actor's frame.
	 * ModuleToWorld is identity for a source map opened directly; callers must supply the
	 * actual level instance transform for an instanced map. Fails without partial output.
	 */
	bool ReadModuleGeometry(const FTransform& ModuleToWorld,
		TArray<FHBDungeonConnectorDefinition>& OutConnectors,
		TArray<FHBDungeonOccupancyDefinition>& OutOccupancy,
		TArray<FHBDungeonModuleIssue>& OutIssues) const;
};
