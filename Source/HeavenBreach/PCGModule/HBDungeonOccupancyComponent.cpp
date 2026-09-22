#include "PCGModule/HBDungeonOccupancyComponent.h"

UHBDungeonOccupancyComponent::UHBDungeonOccupancyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InitBoxExtent(FVector(100));
	ShapeColor = FColor(60, 220, 100);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
}

void UHBDungeonOccupancyComponent::OnRegister()
{
	// Also enforce on older BP defaults / pasted components before creating physics state.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	Super::OnRegister();
}
