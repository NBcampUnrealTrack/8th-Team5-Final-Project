#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PCGModule/HBEncounterComponent.h"
#include "HBEncounterSubsystem.generated.h"

class AEnemyCharacterBase;
class AHBEnemySpawnPoint;
class AHBEncounterSetupActor;
class ULevel;

struct FHBEncounterSpawnChoice
{
	FName PointId;
	AHBEnemySpawnPoint* Point = nullptr;
	TSubclassOf<AEnemyCharacterBase> EnemyClass;
	EHBEncounterGrade Grade = EHBEncounterGrade::Common;
};

namespace HBEncounterSelection
{
	/** Stable point ordering and a room-specific stream make every draw replayable. */
	HEAVENBREACH_API bool SelectAndPlace(const UHBEncounterComponent& Config,
		const TArray<AHBEnemySpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
		TFunctionRef<bool(const FHBEncounterSpawnChoice&, FString&)> TryPlace,
		TArray<FHBEncounterSpawnChoice>& OutPlaced, FString& OutError);
}

namespace HBEncounterPlacement
{
	/** Preserve horizontal spacing only while the capsules' vertical extents overlap. */
	HEAVENBREACH_API bool AreCapsulesTooClose(const FVector& Location, float Radius, float HalfHeight,
		const FVector& OtherLocation, float OtherRadius, float OtherHalfHeight, float MinSpacing);
}

/** One per game world; all encounter execution stays on the authority world. */
UCLASS()
class HEAVENBREACH_API UHBEncounterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static bool HasEncounter(const ULevel* RoomLevel);
	/** Dungeon preparation defers possession so AI cannot run before match start. */
	bool StartEncounter(int32 GenerationId, int32 DungeonSeed, int32 RoomId, ULevel* RoomLevel,
		const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError, bool bDeferActivation = false);
	bool ActivateGeneration(int32 GenerationId, FString& OutError);
	bool IsPreparedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const;
	bool IsActivatedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const;
	int32 GetAliveCount(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const;
	void ClearGeneration(int32 GenerationId);
	virtual void Deinitialize() override;

private:
	struct FRoomState
	{
		TWeakObjectPtr<ULevel> Level;
		int32 GenerationId = INDEX_NONE;
		int32 RoomId = INDEX_NONE;
		int32 DungeonSeed = 0;
		int32 AliveCount = 0;
		bool bPrepared = false;
		bool bDeferActivation = false;
		bool bActivated = false;
		TArray<TWeakObjectPtr<AEnemyCharacterBase>> SpawnedEnemies;
		TSet<TWeakObjectPtr<AEnemyCharacterBase>> CountedDeaths;
	};

	bool CanManageRoomSpawns() const;
	bool TrySpawn(const FHBEncounterSpawnChoice& Choice, const UHBEncounterComponent& Config,
		AHBEncounterSetupActor& Setup, ULevel& RoomLevel, const FBox& RoomBounds,
		const TArray<FVector>& EntryLocations, const FRoomState& State,
		AEnemyCharacterBase*& OutEnemy, FString& OutReason);
	void HandleEnemyDied(AEnemyCharacterBase* Enemy);
	void ClearRoom(FRoomState& State);
	TArray<FRoomState> RoomStates;
};
