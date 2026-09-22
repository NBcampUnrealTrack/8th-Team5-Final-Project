#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBDungeonMarkerActors.generated.h"

class UHBDungeonConnectorComponent;
class UHBDungeonOccupancyComponent;

/** Place independently at each exterior doorway / corridor end in the module level. */
UCLASS(Blueprintable, meta=(DisplayName="HB Dungeon Connector"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBDungeonConnector : public AActor
{
	GENERATED_BODY()
public:
	AHBDungeonConnector();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon")
	TObjectPtr<UHBDungeonConnectorComponent> Connector;
};

/** Place independently; use multiple boxes for complex module shapes. */
UCLASS(Blueprintable, meta=(DisplayName="HB Dungeon Occupancy"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBDungeonOccupancy : public AActor
{
	GENERATED_BODY()
public:
	AHBDungeonOccupancy();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon")
	TObjectPtr<UHBDungeonOccupancyComponent> Occupancy;
};
