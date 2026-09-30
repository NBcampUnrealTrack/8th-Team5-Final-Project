#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HBRoomEncounterProfile.generated.h"

class AEnemyCharacterBase;

UENUM(BlueprintType)
enum class EHBEnemySpawnGrade : uint8
{
	Common,
	Elite,
	Boss
};

USTRUCT(BlueprintType)
struct FHBEnemySpawnOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	TSubclassOf<AEnemyCharacterBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="1"))
	int32 Weight = 1;
};

/** Authoring rules for one room module. Class names are never inferred from map names. */
UCLASS(BlueprintType)
class HEAVENBREACH_API UHBRoomEncounterProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Common", meta=(ClampMin="0"))
	int32 MinCommonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Common", meta=(ClampMin="0"))
	int32 MaxCommonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Common")
	TArray<FHBEnemySpawnOption> CommonEnemies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Elite", meta=(ClampMin="0"))
	int32 MinEliteCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Elite", meta=(ClampMin="0"))
	int32 MaxEliteCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Elite")
	TArray<FHBEnemySpawnOption> EliteEnemies;

	/** Bosses use a dedicated point and are never part of weighted normal/elite draws. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Boss")
	TSubclassOf<AEnemyCharacterBase> BossClass;

	/** Exact PointId of the boss marker in the source room map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Boss")
	FName BossPointId;

	/** Returns authoring errors; runtime placement checks happen after streaming. */
	bool Validate(TArray<FString>& OutErrors) const;
};
