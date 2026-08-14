#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.h"

DECLARE_DELEGATE_TwoParams(FOnStashLoaded, bool, const TArray<FInventoryEntry>&);
DECLARE_DELEGATE_OneParam(FOnStashSaved, bool);

class HEAVENBREACH_API IStashStorage
{
public:
	virtual ~IStashStorage() = default;

	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual void LoadStashAsync(const FString& InAccountId, FOnStashLoaded InCallback) = 0;
	virtual void SaveStashAsync(const FString& InAccountId, const TArray<FInventoryEntry>& InEntries, FOnStashSaved InCallback) = 0;
};