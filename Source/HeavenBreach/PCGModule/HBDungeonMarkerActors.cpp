#include "PCGModule/HBDungeonMarkerActors.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"

AHBDungeonConnector::AHBDungeonConnector()
{
	PrimaryActorTick.bCanEverTick = false;
	Connector = CreateDefaultSubobject<UHBDungeonConnectorComponent>(TEXT("Connector"));
	SetRootComponent(Connector);
}

AHBDungeonOccupancy::AHBDungeonOccupancy()
{
	PrimaryActorTick.bCanEverTick = false;
	Occupancy = CreateDefaultSubobject<UHBDungeonOccupancyComponent>(TEXT("Occupancy"));
	SetRootComponent(Occupancy);
}
