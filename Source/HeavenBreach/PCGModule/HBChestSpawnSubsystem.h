#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PCGModule/HBChestPlacementComponent.h"
#include "PCGModule/HBRoomSpawnEntry.h"
#include "HBChestSpawnSubsystem.generated.h"

class ABoxActor;
class AHBChestSetupActor;
class AHBChestSpawnPoint;
class ULevel;
struct FHBSpawnSelectionReport;
struct FHBSpawnPlacementReport;

struct FHBChestSpawnChoice
{
	FName PointId;
	AHBChestSpawnPoint* Point = nullptr;
	TSubclassOf<ABoxActor> ChestClass;
	bool bRequireEntryPath = true;
	FName NavigationAnchorId;
};

namespace HBChestSelection
{
	/** Stable point IDs and a per-room stream make count, class, and point draws replayable. */
	HEAVENBREACH_API bool SelectAndPlace(const UHBChestPlacementComponent& Config,
		const TArray<AHBChestSpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
		TFunctionRef<bool(const FHBChestSpawnChoice&, FString&)> TryPlace,
		TArray<FHBChestSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport = nullptr);

	HEAVENBREACH_API bool SelectAndPlaceWithDiagnostics(const UHBChestPlacementComponent& Config,
		const TArray<AHBChestSpawnPoint*>& Points, int32 DungeonSeed, int32 RoomId,
		TFunctionRef<bool(const FHBChestSpawnChoice&, FString&, FHBSpawnPlacementReport&)> TryPlace,
		TArray<FHBChestSpawnChoice>& OutPlaced, FString& OutError, FHBSpawnSelectionReport* OutReport = nullptr);
}

/** One per world. Only the authority world may choose or spawn room chests. */
UCLASS()
class HEAVENBREACH_API UHBChestSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static bool HasPlacement(const ULevel* RoomLevel);
	bool StartPlacement(int32 GenerationId, int32 DungeonSeed, int32 RoomId, ULevel* RoomLevel,
		const FBox& RoomBounds, const TArray<FVector>& EntryLocations, FString& OutError);
	/** Separate entry spawn spacing from the navigation path target. */
	bool StartPlacementWithEntries(int32 GenerationId, int32 DungeonSeed, int32 RoomId, ULevel* RoomLevel,
		const FBox& RoomBounds, const TArray<FHBRoomSpawnEntry>& Entries, FString& OutError);
	bool IsPreparedFor(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const;
	int32 GetSpawnedCount(int32 GenerationId, int32 RoomId, const ULevel* RoomLevel) const;
	void ClearGeneration(int32 GenerationId);
	virtual void Deinitialize() override;

private:
	struct FRoomState
	{
		TWeakObjectPtr<ULevel> Level;
		int32 GenerationId = INDEX_NONE;
		int32 RoomId = INDEX_NONE;
		int32 DungeonSeed = 0;
		bool bPrepared = false;
		TMap<FName, FVector> NavigationAnchors;
		TArray<TWeakObjectPtr<ABoxActor>> SpawnedChests;
	};

	bool CanManageRoomSpawns() const;
	bool TrySpawn(const FHBChestSpawnChoice& Choice, const UHBChestPlacementComponent& Config,
		AHBChestSetupActor& Setup, ULevel& RoomLevel, const FBox& RoomBounds,
		const TArray<FHBRoomSpawnEntry>& Entries, const FRoomState& State,
		ABoxActor*& OutChest, FString& OutReason, FHBSpawnPlacementReport& PlacementReport);
	void ClearRoom(FRoomState& State);
	TArray<FRoomState> RoomStates;
};
