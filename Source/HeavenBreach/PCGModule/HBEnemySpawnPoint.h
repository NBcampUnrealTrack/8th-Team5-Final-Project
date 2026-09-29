#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBEnemySpawnPoint.generated.h"

class AEnemyCharacterBase;
class UArrowComponent;

/** LD-authored floor marker. The room encounter system selects among these points. */
UCLASS(Blueprintable, meta=(DisplayName="HB Enemy Spawn Point"), HideCategories=(Replication, Networking, Input, Physics, Collision))
class HEAVENBREACH_API AHBEnemySpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AHBEnemySpawnPoint();

	/** Stable within the source room map; never use transient actor names for random selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	FName PointId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	bool bAllowCommon = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	bool bAllowElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	bool bAllowBoss = false;

	/** Optional exact-class restriction; an empty list allows every class of the permitted grade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	TArray<TSubclassOf<AEnemyCharacterBase>> AllowedClasses;

	/** Zero means no authored size limit. Actual capsule and collision are checked at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point", meta=(ClampMin="0"))
	float MaxCapsuleRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point", meta=(ClampMin="0"))
	float MaxCapsuleHalfHeight = 0.f;

	/** A required point must be filled; its grade and class still come from the room profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	bool bRequired = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Point")
	TObjectPtr<UArrowComponent> Direction;
};
