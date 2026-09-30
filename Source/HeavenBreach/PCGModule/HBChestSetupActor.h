#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBChestSetupActor.generated.h"

class UHBChestPlacementComponent;

/** Place one in a source room. All configuration lives on Placement. */
UCLASS(Blueprintable, meta=(DisplayName="HB Chest Setup"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBChestSetupActor : public AActor
{
	GENERATED_BODY()
public:
	AHBChestSetupActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest Placement")
	TObjectPtr<UHBChestPlacementComponent> Placement;
};
