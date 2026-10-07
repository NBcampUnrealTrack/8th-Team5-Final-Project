#include "PCGModule/HBSpawnNavigationAnchor.h"
#include "Components/ArrowComponent.h"

AHBSpawnNavigationAnchor::AHBSpawnNavigationAnchor()
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
