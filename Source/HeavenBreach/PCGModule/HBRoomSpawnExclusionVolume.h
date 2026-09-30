#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBRoomSpawnExclusionVolume.generated.h"

class UBoxComponent;

/** Authored no-spawn box for door paths, portal arrivals and other required space. */
UCLASS(Blueprintable, meta=(DisplayName="HB Room Spawn Exclusion"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBRoomSpawnExclusionVolume : public AActor
{
	GENERATED_BODY()
public:
	AHBRoomSpawnExclusionVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Exclusion")
	TObjectPtr<UBoxComponent> Bounds;

	virtual void OnConstruction(const FTransform& Transform) override;
};
