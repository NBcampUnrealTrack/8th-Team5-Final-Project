#include "PCGModule/HBSpawnCountSelection.h"

bool HBSpawnCountSelection::Validate(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
	const TArray<FHBSpawnCountWeight>& Weights, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (MinCount < 0 || MaxCount < MinCount)
		OutErrors.Add(TEXT("count range must satisfy 0 <= MinCount <= MaxCount."));
	if (Mode == EHBSpawnCountSelectionMode::Uniform)
	{
		if (static_cast<int64>(MaxCount) - MinCount + 1 > MAX_int32)
			OutErrors.Add(TEXT("uniform count range exceeds int32 random range."));
		return OutErrors.IsEmpty();
	}
	if (Mode != EHBSpawnCountSelectionMode::Weighted)
	{
		OutErrors.Add(TEXT("count selection mode is invalid."));
		return false;
	}
	TSet<int32> Counts;
	int64 TotalWeight = 0;
	for (const FHBSpawnCountWeight& Entry : Weights)
	{
		if (Entry.Count < MinCount || Entry.Count > MaxCount)
			OutErrors.Add(FString::Printf(TEXT("weighted count %d is outside MinCount / MaxCount."), Entry.Count));
		if (Counts.Contains(Entry.Count))
			OutErrors.Add(FString::Printf(TEXT("weighted count %d appears more than once."), Entry.Count));
		Counts.Add(Entry.Count);
		if (Entry.Weight < 0)
			OutErrors.Add(FString::Printf(TEXT("count %d has a negative weight."), Entry.Count));
		else TotalWeight += Entry.Weight;
	}
	if (TotalWeight == 0) OutErrors.Add(TEXT("weighted count selection needs at least one positive weight."));
	if (TotalWeight > MAX_int32) OutErrors.Add(TEXT("total count weight exceeds int32 range."));
	return OutErrors.IsEmpty();
}

bool HBSpawnCountSelection::GetSelectableRange(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
	const TArray<FHBSpawnCountWeight>& Weights, int32& OutMinimum, int32& OutMaximum)
{
	OutMinimum = OutMaximum = 0;
	TArray<FString> Errors;
	if (!Validate(MinCount, MaxCount, Mode, Weights, Errors)) return false;
	if (Mode == EHBSpawnCountSelectionMode::Uniform)
	{
		OutMinimum = MinCount;
		OutMaximum = MaxCount;
		return true;
	}
	OutMinimum = MAX_int32;
	for (const FHBSpawnCountWeight& Entry : Weights)
		if (Entry.Weight > 0)
		{
			OutMinimum = FMath::Min(OutMinimum, Entry.Count);
			OutMaximum = FMath::Max(OutMaximum, Entry.Count);
		}
	return true;
}

bool HBSpawnCountSelection::Select(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
	const TArray<FHBSpawnCountWeight>& Weights, FRandomStream& Random, int32& OutCount, FString& OutError)
{
	OutCount = 0;
	OutError.Reset();
	TArray<FString> Errors;
	if (!Validate(MinCount, MaxCount, Mode, Weights, Errors))
	{
		OutError = FString::Join(Errors, TEXT("; "));
		return false;
	}
	if (Mode == EHBSpawnCountSelectionMode::Uniform)
	{
		OutCount = Random.RandRange(MinCount, MaxCount);
		return true;
	}
	TArray<FHBSpawnCountWeight> Candidates;
	int32 TotalWeight = 0;
	for (const FHBSpawnCountWeight& Entry : Weights)
		if (Entry.Weight > 0)
		{
			Candidates.Add(Entry);
			TotalWeight += Entry.Weight;
		}
	Candidates.Sort([](const FHBSpawnCountWeight& A, const FHBSpawnCountWeight& B) { return A.Count < B.Count; });
	int32 Roll = Random.RandRange(1, TotalWeight);
	for (const FHBSpawnCountWeight& Entry : Candidates)
	{
		Roll -= Entry.Weight;
		if (Roll <= 0)
		{
			OutCount = Entry.Count;
			return true;
		}
	}
	OutError = TEXT("Count weight selection produced no result.");
	return false;
}
