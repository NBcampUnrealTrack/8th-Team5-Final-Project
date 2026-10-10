#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBPortalSpawnPoint.generated.h"

class UArrowComponent;

/** LD-authored portal candidate. Its floor and counterpart are resolved by the dungeon generator. */
UCLASS(Blueprintable, meta=(DisplayName="HB Portal Spawn Point"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBPortalSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AHBPortalSpawnPoint();

	/** Stable and unique within the source room map; reused room instances may share this ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal Placement|Point")
	FName PointId;

	/** Portal actor origin and orientation. Arrival remains relative to the selected portal class. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal Placement|Point")
	TObjectPtr<UArrowComponent> Direction;

	bool ValidateDefinition(FString& Error) const;
};
