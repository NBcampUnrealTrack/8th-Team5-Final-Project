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

	/** Empty means every box class in the matching group is allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	TArray<TSubclassOf<ABoxActor>> AllowedClasses;

	/** A required point must be filled during placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	bool bRequired = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest Placement|Point")
	TObjectPtr<UArrowComponent> Direction;
};
