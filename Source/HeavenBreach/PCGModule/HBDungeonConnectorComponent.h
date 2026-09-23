#pragma once
#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "HBDungeonConnectorComponent.generated.h"

/** Floor-center connection marker. +X points outward, +Z up; unit scale. */
UCLASS(ClassGroup=(Dungeon), meta=(BlueprintSpawnableComponent), BlueprintType,
	HideCategories=(Collision, Physics, Lighting, Navigation, Rendering, HLOD, ArrowComponent))
class HEAVENBREACH_API UHBDungeonConnectorComponent : public UArrowComponent
{
	GENERATED_BODY()
public:
	UHBDungeonConnectorComponent();
	/** Optional explicit ID. Empty uses saved actor/component names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Connector")
	FName ConnectorId;
	FName GetEffectiveConnectorId() const;
};
