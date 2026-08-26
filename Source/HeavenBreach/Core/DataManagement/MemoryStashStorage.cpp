#include "MemoryStashStorage.h"

bool FMemoryStashStorage::Initialize()
{
	Stashes.Reset();
	return true;
}

void FMemoryStashStorage::Shutdown()
{
	Stashes.Reset();
}

void FMemoryStashStorage::LoadStashAsync(const FString& InAccountId, FOnStashLoaded InCallback)
{
	const TArray<FInventoryEntry>* Found = Stashes.Find(InAccountId);

	if (Found == nullptr)
	{
		InCallback.ExecuteIfBound(true, TArray<FInventoryEntry>());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Stash: loaded %d entries for %s"), Found->Num(), *InAccountId);

	InCallback.ExecuteIfBound(true, *Found);
}

void FMemoryStashStorage::SaveStashAsync(const FString& InAccountId, const TArray<FInventoryEntry>& InEntries, FOnStashSaved InCallback)
{
	Stashes.Add(InAccountId, InEntries);

	UE_LOG(LogTemp, Log, TEXT("Stash: saved %d entries for %s"), InEntries.Num(), *InAccountId);

	InCallback.ExecuteIfBound(true);
}