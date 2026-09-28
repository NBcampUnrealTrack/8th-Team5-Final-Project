#pragma once

#include "CoreMinimal.h"
#include "HBDungeonGenerationRules.generated.h"

/** User-selected modules and portal IDs. Counts exclude the mandatory boss corridor. */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBDungeonGenerationRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition") TSoftObjectPtr<UWorld> Floor1ExitMap;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition") FName Floor1ExitPortalId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition") TSoftObjectPtr<UWorld> Floor2EntryMap;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition") FName Floor2EntryPortalId;
	/** Offset of the floor-2 entry module in generator space, not a gameplay floor-height test. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition") FVector Floor2Offset = FVector(100000, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss") TSoftObjectPtr<UWorld> BossCorridorMap;
	/** Includes the starting room and selected exit room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(ClampMin="2", ClampMax="24")) int32 Floor1MainRooms = 4;
	/** Includes the selected entry room and boss, but not the corridor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(ClampMin="3", ClampMax="24")) int32 Floor2MainRooms = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(ClampMin="0", ClampMax="8")) int32 Floor1BranchRooms = 1;
	/** Must exceed Floor1BranchRooms, as required by the dungeon rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(ClampMin="1", ClampMax="8")) int32 Floor2BranchRooms = 2;
};
