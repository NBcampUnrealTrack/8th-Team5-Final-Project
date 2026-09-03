#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.h"

struct FPlayerLoadout
{
	TArray<FInventoryEntry> InventoryEntries;
	TArray<FItemInstance> EquipmentSlots;
	TArray<FGuid> QuickSlots;
};

DECLARE_DELEGATE_TwoParams(FOnLoadoutLoaded, bool, const FPlayerLoadout&);
DECLARE_DELEGATE_OneParam(FOnLoadoutSaved, bool);

class HEAVENBREACH_API ILoadoutStorage
{
public:
	virtual ~ILoadoutStorage() = default;

	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual void LoadLoadoutAsync(const FString& InAccountId, FOnLoadoutLoaded InCallback) = 0;
	virtual void SaveLoadoutAsync(const FString& InAccountId, const FPlayerLoadout& InLoadout, FOnLoadoutSaved InCallback) = 0;
};