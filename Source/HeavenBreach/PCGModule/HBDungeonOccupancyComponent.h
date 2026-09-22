#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "HBDungeonOccupancyComponent.generated.h"

/** Authoring exclusion volume. Never substitutes for the room's physical collision. */
UCLASS(ClassGroup=(Dungeon), meta=(BlueprintSpawnableComponent), BlueprintType,
	HideCategories=(Collision, Physics, Navigation, Lighting, HLOD))
class HEAVENBREACH_API UHBDungeonOccupancyComponent : public UBoxComponent
{
	GENERATED_BODY()
public:
	UHBDungeonOccupancyComponent();
protected:
	virtual void OnRegister() override;
};
