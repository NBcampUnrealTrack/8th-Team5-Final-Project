#include "PCGModule/HBDungeonConnectorComponent.h"
#include "GameFramework/Actor.h"

UHBDungeonConnectorComponent::UHBDungeonConnectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ArrowColor = FColor(0, 200, 255);
	ArrowLength = 150;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
}

FVector UHBDungeonConnectorComponent::GetNavigationEntryLocation() const
{
	return GetComponentTransform().TransformPosition(NavigationEntryOffset);
}

FName UHBDungeonConnectorComponent::GetEffectiveConnectorId() const
{
	if (!ConnectorId.IsNone()) { return ConnectorId; }
	return GetOwner() ? FName(*(GetOwner()->GetName() + TEXT(".") + GetName())) : NAME_None;
}
