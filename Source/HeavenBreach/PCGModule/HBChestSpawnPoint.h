#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBChestSpawnPoint.generated.h"

class ABoxActor;
class UArrowComponent;

/** LD-authored floor marker; the chest subsystem will use points from this room's level only. */
UCLASS(Blueprintable, meta=(DisplayName="HB Chest Spawn Point"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBChestSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AHBChestSpawnPoint();

	/** Stable and unique within the source room map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	FName PointId;

	/** Empty means any compatible group may use this point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	FName GroupId;

	/** Override the group's path target. An empty override ID explicitly uses active room entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Navigation")
	bool bOverrideNavigationAnchor = false;

	/** Room-owned target used when overriding the group. Explicit IDs never fall back to entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Navigation", meta=(EditCondition="bOverrideNavigationAnchor"))
	FName NavigationAnchorId;

	/** Empty means every box class in the matching group is allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	TArray<TSubclassOf<ABoxActor>> AllowedClasses;

	/** When disabled only the authored position is checked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Position Correction")
	bool bAllowPositionCorrection = true;

	/** Opt in to the selected root mesh's scaled horizontal footprint; otherwise retain 10/20/30 cm rings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Position Correction", meta=(EditCondition="bAllowPositionCorrection"))
	bool bUseMeshBasedCorrection = false;

	/** Maximum horizontal adjustment = horizontal mesh bounding radius * multiplier. Zero checks only the marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Position Correction", meta=(ClampMin="0", UIMax="4", EditCondition="bAllowPositionCorrection && bUseMeshBasedCorrection"))
	float CorrectionRadiusMultiplier = 2.f;

	/** A required point must be filled during placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	bool bRequired = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	TObjectPtr<UArrowComponent> Direction;
};
