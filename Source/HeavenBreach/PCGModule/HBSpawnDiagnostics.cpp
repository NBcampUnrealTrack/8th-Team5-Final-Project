#include "PCGModule/HBSpawnDiagnostics.h"

namespace
{
	const TCHAR* ReasonName(EHBSpawnFailureReason Reason)
	{
		switch (Reason)
		{
		case EHBSpawnFailureReason::None: return TEXT("None");
		case EHBSpawnFailureReason::PlacementRejected: return TEXT("PlacementRejected");
		case EHBSpawnFailureReason::NoCompatibleClass: return TEXT("NoCompatibleClass");
		case EHBSpawnFailureReason::GroupMismatch: return TEXT("GroupMismatch");
		case EHBSpawnFailureReason::GradeMismatch: return TEXT("GradeMismatch");
		case EHBSpawnFailureReason::TargetReached: return TEXT("TargetReached");
		case EHBSpawnFailureReason::PointNotAttempted: return TEXT("PointNotAttempted");
		case EHBSpawnFailureReason::RequiredPointUnfilled: return TEXT("RequiredPointUnfilled");
		case EHBSpawnFailureReason::MinCountNotMet: return TEXT("MinCountNotMet");
		case EHBSpawnFailureReason::InvalidConfiguration: return TEXT("InvalidConfiguration");
		}
		return TEXT("Unknown");
	}

	FString FormatPoint(const FHBSpawnPointReport& Point, const FName* OnlyGroup = nullptr)
	{
		TArray<FString> Results;
		for (const FHBSpawnClassFailure& Failure : Point.ClassFailures)
		{
			if (OnlyGroup && Failure.GroupId != *OnlyGroup) continue;
			Results.Add(FString::Printf(TEXT("Group=%s Class=%s Cause=%s Detail={%s}"),
				*Failure.GroupId.ToString(), *Failure.ClassPath, ReasonName(Failure.Cause), *Failure.Details));
		}
		for (const FHBSpawnPointSkip& Skip : Point.Skips)
		{
			if (OnlyGroup && Skip.GroupId != *OnlyGroup) continue;
			Results.Add(FString::Printf(TEXT("Group=%s NotAttempted=%s"),
				*Skip.GroupId.ToString(), ReasonName(Skip.Reason)));
		}
		if (Results.IsEmpty()) Results.Add(TEXT("NotAttempted=PointNotAttempted"));
		return FString::Printf(TEXT("Point=%s Required=%d Placed=%d Results=[%s]"),
			*Point.PointId.ToString(), Point.bRequired ? 1 : 0, Point.bPlaced ? 1 : 0,
			*FString::Join(Results, TEXT("; ")));
	}
}

void FHBSpawnSelectionReport::AddPoint(FName PointId, bool bRequired)
{
	FHBSpawnPointReport& Point = Points.FindOrAdd(PointId);
	Point.PointId = PointId;
	Point.bRequired = bRequired;
}

void FHBSpawnSelectionReport::RecordSkip(FName PointId, FName GroupId, EHBSpawnFailureReason Reason)
{
	Points.FindChecked(PointId).Skips.Add({GroupId, Reason});
}

void FHBSpawnSelectionReport::RecordClassFailure(FName PointId, FName GroupId,
	const FString& ClassPath, const FString& Details)
{
	Points.FindChecked(PointId).ClassFailures.Add({GroupId, ClassPath,
		EHBSpawnFailureReason::PlacementRejected,
		Details.IsEmpty() ? FString(TEXT("Placement callback supplied no failure detail.")) : Details});
}

void FHBSpawnSelectionReport::RecordPlaced(FName PointId)
{
	Points.FindChecked(PointId).bPlaced = true;
}

FString FHBSpawnSelectionReport::FormatRequiredFailure(FName PointId, bool bChest) const
{
	const FHBSpawnPointReport* Point = Points.Find(PointId);
	return FString::Printf(TEXT("Room %d required %spoint %s was not filled. FailureReason=RequiredPointUnfilled Seed=%d %s"),
		RoomId, bChest ? TEXT("chest ") : TEXT(""), *PointId.ToString(), DungeonSeed,
		Point ? *FormatPoint(*Point) : TEXT("NotAttempted=PointNotAttempted"));
}

FString FHBSpawnSelectionReport::FormatGroupResults(FName GroupId) const
{
	TArray<FName> PointIds;
	Points.GetKeys(PointIds);
	PointIds.Sort([](FName A, FName B) { return A.ToString() < B.ToString(); });
	TArray<FString> Results;
	for (FName PointId : PointIds)
	{
		const FHBSpawnPointReport& Point = Points.FindChecked(PointId);
		if (Point.ClassFailures.ContainsByPredicate([GroupId](const FHBSpawnClassFailure& Failure)
			{ return Failure.GroupId == GroupId; })
			|| Point.Skips.ContainsByPredicate([GroupId](const FHBSpawnPointSkip& Skip)
				{ return Skip.GroupId == GroupId; }))
			Results.Add(FormatPoint(Point, &GroupId));
	}
	return FString::Printf(TEXT("PointResults=[%s]"), *FString::Join(Results, TEXT(" | ")));
}
