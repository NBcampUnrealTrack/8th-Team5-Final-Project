#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBDungeonDoorSeal.generated.h"
class UStaticMeshComponent;
/** Visible blocking wall for an unused connector. Replace the mesh in a Blueprint subclass. */
UCLASS(Blueprintable)
class HEAVENBREACH_API AHBDungeonDoorSeal : public AActor
{
    GENERATED_BODY()
public:
    AHBDungeonDoorSeal();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon") TObjectPtr<UStaticMeshComponent> Wall;
    void SetExtent(const FVector& HalfExtent);
};
