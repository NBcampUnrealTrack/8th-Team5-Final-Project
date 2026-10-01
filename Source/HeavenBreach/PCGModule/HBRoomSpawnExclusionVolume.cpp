#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "Components/BoxComponent.h"

AHBRoomSpawnExclusionVolume::AHBRoomSpawnExclusionVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("NoSpawnBounds"));
	SetRootComponent(Bounds);
	Bounds->InitBoxExtent(FVector(150.f, 150.f, 150.f));
	Bounds->ShapeColor = FColor(230, 65, 65);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetCanEverAffectNavigation(false);
	Bounds->SetHiddenInGame(true);
}

void AHBRoomSpawnExclusionVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetCanEverAffectNavigation(false);
}
