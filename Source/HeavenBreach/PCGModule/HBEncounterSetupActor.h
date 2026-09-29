#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBEncounterSetupActor.generated.h"

class UHBEncounterComponent;

/** Place one in a room to author its encounter. All settings live on Encounter. */
UCLASS(Blueprintable, meta=(DisplayName="HB Encounter Setup"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBEncounterSetupActor : public AActor
{
	GENERATED_BODY()
public:
	AHBEncounterSetupActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter")
	TObjectPtr<UHBEncounterComponent> Encounter;
};
