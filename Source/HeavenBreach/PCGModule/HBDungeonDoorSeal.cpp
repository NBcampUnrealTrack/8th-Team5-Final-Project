#include "PCGModule/HBDungeonDoorSeal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
AHBDungeonDoorSeal::AHBDungeonDoorSeal()
{
    Wall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wall"));
    RootComponent = Wall;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Wall->SetStaticMesh(Cube.Object);
    Wall->SetCollisionProfileName(TEXT("BlockAll"));
    Wall->SetGenerateOverlapEvents(false);
}
void AHBDungeonDoorSeal::SetExtent(const FVector& HalfExtent)
{
    if (const UStaticMesh* Mesh = Wall->GetStaticMesh())
        Wall->SetRelativeScale3D(HalfExtent / Mesh->GetBounds().BoxExtent.ComponentMax(FVector(0.01)));
}
