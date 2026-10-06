#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FCollisionQueryParams;
struct FCollisionShape;
struct FHitResult;
struct FOverlapResult;

/** Aggregate selection failures and individual placement causes have separate report fields. */
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
	InvalidConfiguration,
	CapsuleOverlap
};

struct FHBSpawnBlockingTarget
{
	FString ActorName;
	FString ActorPath;
	FString ComponentName;
	FString ComponentPath;
};

/** A rejected capsule check, not an index into the complete ground candidate sequence. */
struct HEAVENBREACH_API FHBSpawnCapsuleFailure
{
	EHBSpawnFailureReason Cause = EHBSpawnFailureReason::CapsuleOverlap;
	int32 CapsuleCheckIndex = INDEX_NONE;
	FVector Marker = FVector::ZeroVector;
	FVector GroundLocation = FVector::ZeroVector;
	FVector SpawnLocation = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	float Radius = 0.f;
	float HalfHeight = 0.f;
	FName Profile;
	FString GroundActor;
	FString GroundComponent;
	FString GroundActorPath;
	FString GroundComponentPath;
	bool bMultiHasBlocking = false;
	int32 UnresolvedBlockingResults = 0;
	TArray<FHBSpawnBlockingTarget> BlockingTargets;

	void SetOverlaps(bool bHasBlocking, const TArray<FOverlapResult>& Overlaps);
	bool HasQueryMismatch() const;
	FString Format() const;
};

struct HEAVENBREACH_API FHBSpawnPlacementReport
{
	int32 CapsuleChecks = 0;
	TArray<FHBSpawnCapsuleFailure> CapsuleFailures;

	FString FormatBlockingSummary() const;
};

namespace HBSpawnDiagnostics
{
	/** Call only after the original bool test rejected this exact query; collection never decides placement. */
	HEAVENBREACH_API FHBSpawnCapsuleFailure CollectCapsuleFailure(UWorld& World,
		const FVector& Marker, const FHitResult& Floor, const FVector& SpawnLocation, const FQuat& Rotation,
		FName Profile, const FCollisionShape& Shape, const FCollisionQueryParams& Query, int32 CapsuleCheckIndex);
}

struct FHBSpawnClassFailure
{
	FName GroupId;
	FString ClassPath;
	EHBSpawnFailureReason Cause = EHBSpawnFailureReason::PlacementRejected;
	FString Details;
	FHBSpawnPlacementReport Placement;
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
	void RecordClassFailure(FName PointId, FName GroupId, const FString& ClassPath, const FString& Details,
		const FHBSpawnPlacementReport* Placement = nullptr);
	void RecordPlaced(FName PointId);
	FString FormatRequiredFailure(FName PointId, bool bChest) const;
	FString FormatGroupResults(FName GroupId) const;
};
