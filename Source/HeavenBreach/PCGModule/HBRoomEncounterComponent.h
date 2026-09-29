#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGModule/HBRoomEncounterProfile.h"
#include "HBRoomEncounterComponent.generated.h"

class AEnemyCharacterBase;
class AHBEnemySpawnPoint;
class AHBDungeonModuleDefinition;

/** One attempted placement. Point locations are already in the streamed world's coordinate frame. */
struct FHBEncounterSpawnChoice
{
	FName PointId;
	AHBEnemySpawnPoint* Point = nullptr;
	TSubclassOf<AEnemyCharacterBase> EnemyClass;
	EHBEnemySpawnGrade Grade = EHBEnemySpawnGrade::Common;
};

namespace HBRoomEncounter
{
	/** Deterministic, bounded selection. TryPlace can reject unsafe candidates and is called on the server. */
	HEAVENBREACH_API bool SelectAndPlace(const UHBRoomEncounterProfile& Profile,
		const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
		TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
		TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError);
}

/** Server-owned encounter state for one streamed room instance. */
UCLASS(ClassGroup=(Dungeon), BlueprintType)
class HEAVENBREACH_API UHBRoomEncounterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHBRoomEncounterComponent();

	/** Authoring distances in cm; tune these with LD on the actual room. */
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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Encounter|Result")
	int32 AliveCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Encounter|Result")
	int32 ActiveRoomId = INDEX_NONE;

	bool StartEncounter(int32 GenerationId, int32 DungeonSeed, int32 RoomId,
		const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError);
	void ClearEncounter();
	bool IsPreparedFor(int32 GenerationId, int32 RoomId) const;

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool TrySpawn(const FHBEncounterSpawnChoice& Choice, const FBox& RoomBounds,
		const TArray<FVector>& EntryLocations, AEnemyCharacterBase*& OutEnemy, FString& OutReason);
	void HandleEnemyDied(AEnemyCharacterBase* Enemy);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyCharacterBase>> SpawnedEnemies;
	TSet<TWeakObjectPtr<AEnemyCharacterBase>> CountedDeaths;
	int32 ActiveGenerationId = INDEX_NONE;
	bool bPrepared = false;
};
