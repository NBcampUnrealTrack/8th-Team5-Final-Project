#include "PCGModule/HBPortalSpawnPoint.h"
#include "Components/ArrowComponent.h"

AHBPortalSpawnPoint::AHBPortalSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("Direction"));
	SetRootComponent(Direction);
	Direction->ArrowColor = FColor::Cyan;
	Direction->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Direction->SetGenerateOverlapEvents(false);
	Direction->SetCanEverAffectNavigation(false);
	Direction->SetHiddenInGame(true);
}

bool AHBPortalSpawnPoint::ValidateDefinition(FString& Error) const
{
	Error.Reset();
	if (PointId.IsNone())
	{
		Error = TEXT("Assign a nonempty portal PointId unique within this room map.");
		return false;
	}
	if (!IsValid(Direction) || !GetActorTransform().IsValid()
		|| !GetActorScale3D().Equals(FVector::OneVector, 0.0001)
		|| !Direction->GetComponentTransform().IsValid()
		|| !Direction->GetComponentScale().Equals(FVector::OneVector, 0.0001))
	{
		Error = TEXT("Portal point transforms must be finite with unit scale; configure the portal's size in its Blueprint.");
		return false;
	}
	return true;
}
