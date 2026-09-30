#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestPlacementComponent.h"
#include "Components/SceneComponent.h"

AHBChestSetupActor::AHBChestSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ChestSetupRoot"));
	Placement = CreateDefaultSubobject<UHBChestPlacementComponent>(TEXT("Placement"));
}
