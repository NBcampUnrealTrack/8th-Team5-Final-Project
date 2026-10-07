#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBSpawnNavigationAnchor.generated.h"

class UArrowComponent;

/** Room-owned navigation target for spawn points inside a separately accessible region. */
UCLASS(Blueprintable, meta=(DisplayName="HB Spawn Navigation Anchor"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBSpawnNavigationAnchor : public AActor
{
	GENERATED_BODY()
public:
	AHBSpawnNavigationAnchor();

	/** Unique within the source room map. Place on navigable ground inside the intended region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn Navigation")
	FName AnchorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawn Navigation")
	TObjectPtr<UArrowComponent> Direction;
};
