#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "HBRoomInfoComponent.generated.h"

/** Fixed authoring role; instance IDs and gameplay state belong to the generator. */
UCLASS(ClassGroup=(Dungeon), meta=(BlueprintSpawnableComponent), BlueprintType)
class HEAVENBREACH_API UHBRoomInfoComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHBRoomInfoComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Room")
	EHBPrototypeRoomRole RoomRole = EHBPrototypeRoomRole::Unassigned;

	/** Allowed logical dungeon tier, independent of the module's world height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Room")
	EHBAllowedFloor AllowedFloor = EHBAllowedFloor::Any;
};
