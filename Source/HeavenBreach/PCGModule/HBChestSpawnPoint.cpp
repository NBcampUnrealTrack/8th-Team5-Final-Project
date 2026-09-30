#include "PCGModule/HBChestSpawnPoint.h"
#include "Components/ArrowComponent.h"

AHBChestSpawnPoint::AHBChestSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("Direction"));
	SetRootComponent(Direction);
	Direction->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Direction->SetGenerateOverlapEvents(false);
	Direction->SetCanEverAffectNavigation(false);
	Direction->SetHiddenInGame(true);
}
