#pragma once

#include "CoreMinimal.h"
#include "HBDungeonLayout.generated.h"

UENUM(BlueprintType)
enum class EHBPrototypeRoomRole : uint8 { Adventure, Recovery, Upgrade, Boss, Unassigned = 255 };

USTRUCT(BlueprintType)
struct FHBPrototypeRoom
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Id = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ParentId = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Floor = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EHBPrototypeRoomRole Role = EHBPrototypeRoomRole::Adventure;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bBranch = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFloorExit = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FTransform Transform;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FBox Bounds = FBox(ForceInit);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ModuleId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TSoftObjectPtr<UWorld> ModuleMap;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FBox LocalBounds = FBox(ForceInit);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector InspectionEntry = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ParentConnectorId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName EntryConnectorId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> UsedConnectorIds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> UnusedConnectorIds;
};

/** Pure planner. Edges describe intended connections, not physical door/corridor geometry. */
namespace HBDungeonLayout
{
	struct FSettings
	{
		int32 Seed = 12345;
		int32 MaxAttempts = 64;
		double CellSize = 16000.0;
		double FloorHeight = 5000.0;
		FBox AdventureBounds = FBox(FVector(-4500.5, -4059.38, -801.54), FVector(4500.5, 4059.38, 1812.0));
		FBox BossBounds = FBox(FVector(-5965, -6899.5, -97.03), FVector(5965, 6899.5, 1723.56));
	};
	HEAVENBREACH_API bool Generate(const FSettings& Settings, TArray<FHBPrototypeRoom>& OutRooms, FString& OutError);
	HEAVENBREACH_API bool Validate(const TArray<FHBPrototypeRoom>& Rooms, FString& OutError);
	HEAVENBREACH_API FString Signature(const TArray<FHBPrototypeRoom>& Rooms);
	HEAVENBREACH_API const TCHAR* RoleName(EHBPrototypeRoomRole Role);
	HEAVENBREACH_API FColor RoleColor(EHBPrototypeRoomRole Role);
}
