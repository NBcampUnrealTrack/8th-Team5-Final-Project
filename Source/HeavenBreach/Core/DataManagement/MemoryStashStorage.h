#pragma once

#include "CoreMinimal.h"
#include "StashStorage.h"

class HEAVENBREACH_API FMemoryStashStorage : public IStashStorage
{
public:
	virtual bool Initialize() override;
	virtual void Shutdown() override;

	virtual void LoadStashAsync(const FString& InAccountId, FOnStashLoaded InCallback) override;
	virtual void SaveStashAsync(const FString& InAccountId, const TArray<FInventoryEntry>& InEntries, FOnStashSaved InCallback) override;

private:
	TMap<FString, TArray<FInventoryEntry>> Stashes;
};