#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGModule/HBSpawnCountSelection.h"
#include "HBEncounterComponent.generated.h"

class AEnemyCharacterBase;

UENUM(BlueprintType)
enum class EHBEncounterGrade : uint8
{
	Common,
	Elite,
	Boss
};

USTRUCT(BlueprintType)
struct FHBEncounterMonsterOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	TSubclassOf<AEnemyCharacterBase> EnemyClass;

	/** Relative chance when this group has more than one eligible monster class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="1"))
	int32 Weight = 1;

	/** Uniform scale for this PCG spawn only; the enemy Blueprint defaults stay unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.01", UIMin="0.1", UIMax="2.0"))
	float SpawnScale = 1.f;
};

USTRUCT(BlueprintType)
struct FHBEncounterGroup
{
	GENERATED_BODY()

	/** Unique within this encounter. A spawn point with no GroupId can serve any compatible group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	EHBEncounterGrade Grade = EHBEncounterGrade::Common;

	/** Set MinCount and MaxCount equal for a fixed number of monsters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0"))
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0"))
	int32 MaxCount = 0;

	/** Uniform preserves the existing inclusive MinCount / MaxCount draw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	EHBSpawnCountSelectionMode CountSelectionMode = EHBSpawnCountSelectionMode::Uniform;

	/** Weighted mode selects only listed counts with positive weight, including MaxCount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter",
		meta=(EditCondition="CountSelectionMode == EHBSpawnCountSelectionMode::Weighted", EditConditionHides, TitleProperty="Count"))
	TArray<FHBSpawnCountWeight> CountWeights;

	/** Optional group placed before this one (higher grade, or same grade listed above). When it placed at least one monster, CountWeightsIfConditionSpawned replaces the normal count draw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Condition")
	FName ConditionGroupId;

	/** Weighted counts used when ConditionGroupId placed at least one monster. Counts must stay inside MinCount / MaxCount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Condition", meta=(TitleProperty="Count"))
	TArray<FHBSpawnCountWeight> CountWeightsIfConditionSpawned;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	TArray<FHBEncounterMonsterOption> Monsters;

	/** Default room-owned path target for this group. Empty uses active room entries. Points can override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Navigation")
	FName NavigationAnchorId;

	/** Require a complete path to the selected anchor or room entry. A region anchor permits checking inside a locked room. Disabling keeps local navigation and placement checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety")
	bool bRequireEntryPath = true;
};

/** LD-authored room encounter data. Runtime selection and spawning belong to the world subsystem. */
UCLASS(ClassGroup=(Dungeon), BlueprintType, meta=(BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBEncounterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHBEncounterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	TArray<FHBEncounterGroup> Groups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety", meta=(ClampMin="0"))
	float MinEntryDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety", meta=(ClampMin="0"))
	float MinMonsterSpacing = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety", meta=(ClampMin="0"))
	float MaxNavProjectionDistance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety", meta=(ClampMin="0"))
	float MaxNavProjectionHeight = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Safety", meta=(ClampMin="0", ClampMax="1"))
	float MinFloorUpDot = 0.7f;

	bool Validate(TArray<FString>& OutErrors) const;
};
