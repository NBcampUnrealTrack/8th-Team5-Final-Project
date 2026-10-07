#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FCollisionQueryParams;
struct FCollisionShape;
struct FHitResult;
struct FOverlapResult;
class UNavigationSystemV1;
class ANavigationData;
struct FNavAgentProperties;
struct FNavLocation;

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
	CapsuleOverlap,
	NoFloor,
	GroundPenetration,
	SteepGround,
	OutsideRoom,
	ExcludedArea,
	EntrySpacing,
	MonsterSpacing,
	ChestSpacing,
	CapsuleSizeExceeded,
	NavProjectionFailed,
	NoCompleteEntryPath,
	SpawnCollision,
	InvalidSpawnedActor
};

enum class EHBSpawnNavStage : uint8 { EnemyGround, ChestApproach, EntryPath };
enum class EHBSpawnCorrectionBasis : uint8 { LegacyFixed, EnemyCapsule, ChestMesh };

struct HEAVENBREACH_API FHBSpawnCorrectionReport
{
	bool bInitialized = false;
	bool bAllowPositionCorrection = true;
	EHBSpawnCorrectionBasis Basis = EHBSpawnCorrectionBasis::LegacyFixed;
	double ShapeRadius = 0.;
	FVector ShapeHalfExtents = FVector::ZeroVector;
	double RadiusMultiplier = 2.;
	double SearchRadius = 0.;
	int32 CandidateBudget = 0;

	FString Format() const;
};
enum class EHBSpawnNavCondition : uint8
{
	None, MissingSystem, MissingAgent, MissingNavData, NavigationBuildingOrLocked,
	ProjectionQueryFailed, HorizontalDistanceExceeded, HeightExceeded, OutsideRoom,
	PathQueryFailed, InvalidPath, PartialPath
};

/** Snapshot of an existing navigation query. No UObject ownership or additional path queries. */
struct HEAVENBREACH_API FHBSpawnNavCheck
{
	EHBSpawnNavStage Stage = EHBSpawnNavStage::EnemyGround;
	EHBSpawnNavCondition Condition = EHBSpawnNavCondition::None;
	int32 EntryIndex = INDEX_NONE;
	bool bHasEntryConnectionLocation = false;
	FVector EntryConnectionLocation = FVector::ZeroVector;
	FVector InputLocation = FVector::ZeroVector;
	FVector ProjectedLocation = FVector::ZeroVector;
	FVector ProjectionExtent = FVector::ZeroVector;
	float AgentRadius = 0.f;
	float AgentHeight = 0.f;
	FString NavDataPath;
	double MaxHorizontalDistance = -1.;
	double MaxHeight = -1.;
	bool bProjectionQueried = false;
	bool bProjectionSucceeded = false;
	bool bPathQueried = false;
	bool bPathSucceeded = false;
	bool bPathValid = false;
	bool bPartialPath = false;
	bool bEntryToPlacement = false;

	bool SetPathResult(bool bSucceeded, bool bValid, bool bPartial);
	FString Format() const;
};

struct HEAVENBREACH_API FHBSpawnCandidateReport
{
	int32 CandidateIndex = INDEX_NONE;
	FVector CandidateLocation = FVector::ZeroVector;
	bool bHasFloor = false;
	FVector GroundLocation = FVector::ZeroVector;
	FVector GroundNormal = FVector::ZeroVector;
	FString GroundActorPath;
	FString GroundComponentPath;
	EHBSpawnFailureReason Cause = EHBSpawnFailureReason::None;
	FString Details;
	int32 CapsuleCheckIndex = INDEX_NONE;
	TArray<FHBSpawnNavCheck> NavigationChecks;
	bool bAccepted = false;

	FString Format() const;
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
	int32 CandidateIndex = INDEX_NONE;
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
	FHBSpawnCorrectionReport Correction;
	/** Pre/post-search failure; an exhausted search uses PlacementRejected, with causes in Candidates. */
	EHBSpawnFailureReason FailureReason = EHBSpawnFailureReason::None;
	FString FailureDetails;
	int32 AcceptedCandidateIndex = INDEX_NONE;
	TArray<FHBSpawnCandidateReport> Candidates;
	int32 CapsuleChecks = 0;
	TArray<FHBSpawnCapsuleFailure> CapsuleFailures;

	void RecordFailure(EHBSpawnFailureReason Reason, const FString& Details);
	TMap<EHBSpawnFailureReason, int32> CountFailureReasons() const;
	FString FormatFailureSummary() const;
	FString FormatBlockingSummary() const;
};

namespace HBSpawnDiagnostics
{
	/** Call only after the original bool test rejected this exact query; collection never decides placement. */
	HEAVENBREACH_API FHBSpawnCapsuleFailure CollectCapsuleFailure(UWorld& World,
		const FVector& Marker, const FHitResult& Floor, const FVector& SpawnLocation, const FQuat& Rotation,
		FName Profile, const FCollisionShape& Shape, const FCollisionQueryParams& Query, int32 CapsuleCheckIndex);
	/** Executes exactly one original projection when prerequisites are available; guards short-circuit in order. */
	HEAVENBREACH_API bool ProjectNavigation(UWorld& World, UNavigationSystemV1* Nav,
		const FNavAgentProperties* Agent, const ANavigationData* NavData,
		const FVector& Input, const FVector& Extent, double MaxDistance, double MaxHeight,
		const FBox* RoomBounds, bool bRejectBuilding, FNavLocation& OutLocation, FHBSpawnNavCheck& Check);
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

struct FHBSpawnClassPlacement
{
	FName GroupId;
	FString ClassPath;
	FHBSpawnPlacementReport Placement;
};

struct FHBSpawnPointReport
{
	FName PointId;
	bool bRequired = false;
	bool bPlaced = false;
	TArray<FHBSpawnClassFailure> ClassFailures;
	/** Retains earlier rejected candidates even when this class eventually places successfully. */
	TArray<FHBSpawnClassPlacement> ClassPlacements;
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
	void RecordPlaced(FName PointId, FName GroupId = NAME_None, const FString& ClassPath = FString(),
		const FHBSpawnPlacementReport* Placement = nullptr);
	TMap<EHBSpawnFailureReason, int32> CountFailureReasons(FName PointId = NAME_None, FName GroupId = NAME_None) const;
	FString FormatRequiredFailure(FName PointId, bool bChest) const;
	FString FormatGroupResults(FName GroupId) const;
};
