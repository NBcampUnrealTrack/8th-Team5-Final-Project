#pragma once

#include "CoreMinimal.h"
#include "StashStorage.h"

class HEAVENBREACH_API FBackendStashStorage : public IStashStorage
{
public:
	virtual bool Initialize() override;
	virtual void Shutdown() override;

	virtual void LoadStashAsync(const FString& InAccountId, FOnStashLoaded InCallback) override;
	virtual void SaveStashAsync(const FString& InAccountId, const TArray<FInventoryEntry>& InEntries, FOnStashSaved InCallback) override;

private:
	FString BuildUrl(const FString& InAccountId) const;

	FString BaseUrl = TEXT("https://heavenbreach-stash-api.onrender.com/api/stash");
};