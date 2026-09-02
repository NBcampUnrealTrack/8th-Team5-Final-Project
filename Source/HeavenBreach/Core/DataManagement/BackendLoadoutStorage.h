#pragma once

#include "CoreMinimal.h"
#include "LoadoutStorage.h"

class HEAVENBREACH_API FBackendLoadoutStorage : public ILoadoutStorage
{
public:
	virtual bool Initialize() override;
	virtual void Shutdown() override;

	virtual void LoadLoadoutAsync(const FString& InAccountId, FOnLoadoutLoaded InCallback) override;
	virtual void SaveLoadoutAsync(const FString& InAccountId, const FPlayerLoadout& InLoadout, FOnLoadoutSaved InCallback) override;

private:
	FString BuildUrl(const FString& InAccountId) const;

	FString BaseUrl = TEXT("https://heavenbreach-stash-api.onrender.com/api/loadout");
};