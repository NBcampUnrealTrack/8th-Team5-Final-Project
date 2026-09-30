#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEncounterComponent.h"
#include "Components/SceneComponent.h"

AHBEncounterSetupActor::AHBEncounterSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EncounterSetupRoot"));
	Encounter = CreateDefaultSubobject<UHBEncounterComponent>(TEXT("Encounter"));
}
