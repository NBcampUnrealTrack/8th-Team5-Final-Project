#pragma once

#include "CoreMinimal.h"

/** Selection failures are separate from the placement callback's physical/navigation details. */
enum class EHBSpawnFailureReason : uint8
{
	None,
	PlacementRejected,
	NoCompatibleClass,
	GroupMismatch,
	GradeMismatch,
	TargetReached,
	PointNotAttempted,
	RequiredPointUnfilled,
	MinCountNotMet,
	InvalidConfiguration
};

struct FHBSpawnClassFailure
{
	FName GroupId;
	FString ClassPath;
	EHBSpawnFailureReason Cause = EHBSpawnFailureReason::PlacementRejected;
	FString Details;
};

struct FHBSpawnPointSkip
{
	FName GroupId;
	EHBSpawnFailureReason Reason = EHBSpawnFailureReason::PointNotAttempted;
};

struct FHBSpawnPointReport
{
	FName PointId;
	bool bRequired = false;
	bool bPlaced = false;
	TArray<FHBSpawnClassFailure> ClassFailures;
	TArray<FHBSpawnPointSkip> Skips;
};

struct FHBSpawnGroupReport
{
	FName GroupId;
	int32 MinCount = 0;
	int32 Target = 0;
	int32 Placed = 0;
	EHBSpawnFailureReason FailureReason = EHBSpawnFailureReason::None;
};

/** Owns names and strings only so failures survive room/actor cleanup. Never drives selection. */
struct HEAVENBREACH_API FHBSpawnSelectionReport
{
	int32 RoomId = INDEX_NONE;
	int32 DungeonSeed = 0;
	EHBSpawnFailureReason FailureReason = EHBSpawnFailureReason::None;
	FName FailedPointId;
	FName FailedGroupId;
	TMap<FName, FHBSpawnPointReport> Points;
	TArray<FHBSpawnGroupReport> Groups;

	void AddPoint(FName PointId, bool bRequired);
	void RecordSkip(FName PointId, FName GroupId, EHBSpawnFailureReason Reason);
	void RecordClassFailure(FName PointId, FName GroupId, const FString& ClassPath, const FString& Details);
	void RecordPlaced(FName PointId);
	FString FormatRequiredFailure(FName PointId, bool bChest) const;
	FString FormatGroupResults(FName GroupId) const;
};
