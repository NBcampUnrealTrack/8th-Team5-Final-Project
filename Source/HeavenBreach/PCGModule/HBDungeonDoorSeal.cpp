#include "PCGModule/HBDungeonDoorSeal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
AHBDungeonDoorSeal::AHBDungeonDoorSeal()
{
    // Each streamed room builds its own walls from the authoritative snapshot.
    bReplicates = false;
    Wall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wall"));
    RootComponent = Wall;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Wall->SetStaticMesh(Cube.Object);
    Wall->SetCollisionProfileName(TEXT("BlockAll"));
    Wall->SetGenerateOverlapEvents(false);
}
namespace
{
    bool HasUsableWall(const AHBDungeonDoorSeal* Seal, const FVector& HalfExtent)
    {
        const UStaticMeshComponent* Wall = Seal ? Seal->Wall.Get() : nullptr;
        const UStaticMesh* Mesh = Wall ? Wall->GetStaticMesh() : nullptr;
        return Seal && !Seal->GetIsReplicated() && !HalfExtent.ContainsNaN() && HalfExtent.GetMin() > 0
            && Mesh && !Mesh->GetBounds().BoxExtent.ContainsNaN() && Mesh->GetBounds().BoxExtent.GetMin() > 0
            && Wall->IsQueryCollisionEnabled() && Wall->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
    }
}
bool AHBDungeonDoorSeal::ValidateSettings(TSubclassOf<AHBDungeonDoorSeal> SealClass, const FVector& HalfExtent, FString& Error)
{
    if (!SealClass || SealClass->HasAnyClassFlags(CLASS_Abstract)
        || !HasUsableWall(SealClass->GetDefaultObject<AHBDungeonDoorSeal>(), HalfExtent))
    {
        Error = TEXT("Door seals require a non-replicated class with a valid mesh, pawn-blocking collision and positive finite extent.");
        return false;
    }
    return true;
}
bool AHBDungeonDoorSeal::SetExtent(const FVector& HalfExtent)
{
    if (!HasUsableWall(this, HalfExtent)) return false;
    Wall->SetRelativeScale3D(HalfExtent / Wall->GetStaticMesh()->GetBounds().BoxExtent);
    return true;
}
