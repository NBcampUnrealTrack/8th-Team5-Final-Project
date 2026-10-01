#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HBChestPlacementComponent.generated.h"

class ABoxActor;

USTRUCT(BlueprintType)
struct FHBChestClassOption
{
	GENERATED_BODY()

	/** Use a fixed-grade box Blueprint; the box still generates its own loot when opened. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement")
	TSubclassOf<ABoxActor> ChestClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement", meta=(ClampMin="1"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct FHBChestPlacementGroup
{
	GENERATED_BODY()

	/** Unique within the room. Points with no GroupId can serve any compatible group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement")
	FName GroupId;

	/** Equal values give a fixed count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement", meta=(ClampMin="0"))
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement", meta=(ClampMin="0"))
	int32 MaxCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement")
	TArray<FHBChestClassOption> Chests;
};

/** LD-authored room settings only. Selection and spawning will belong to a world subsystem. */
UCLASS(ClassGroup=(Dungeon), BlueprintType, meta=(BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBChestPlacementComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHBChestPlacementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement")
	TArray<FHBChestPlacementGroup> Groups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Safety", meta=(ClampMin="0"))
	float MinEntryDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Safety", meta=(ClampMin="0"))
	float MinChestSpacing = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Safety", meta=(ClampMin="0"))
	float MaxNavProjectionDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Safety", meta=(ClampMin="0"))
	float MaxNavProjectionHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Placement|Safety", meta=(ClampMin="0", ClampMax="1"))
	float MinFloorUpDot = 0.7f;

	bool Validate(TArray<FString>& OutErrors) const;
};
