#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBPortalSpawnPoint.generated.h"

class UArrowComponent;

/** LD-authored floor marker. The generator grounds the portal above it and resolves its floor and counterpart. */
UCLASS(Blueprintable, meta=(DisplayName="HB Portal Spawn Point"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBPortalSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AHBPortalSpawnPoint();

	/** Stable and unique within the source room map; reused room instances may share this ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal Placement|Point")
	FName PointId;

	/** Place on the floor with unit scale. Sets portal XY and facing; trigger and arrival heights are grounded at runtime. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal Placement|Point")
	TObjectPtr<UArrowComponent> Direction;

	bool ValidateDefinition(FString& Error) const;
};
