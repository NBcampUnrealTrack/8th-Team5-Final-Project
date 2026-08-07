#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

class UDataTable;

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeGrid(int32 InWidth, int32 InHeight);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FIntPoint GetGridSize() const { return FIntPoint(Width, Height); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FInventoryEntry>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsValidCoord(FIntPoint InCoord) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetEntryIndexAt(FIntPoint InCoord) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindEntryIndexById(FGuid InInstanceId) const;

	bool CanPlaceAt(FIntPoint InSize, FIntPoint InPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	bool FindFirstFreeSlot(FIntPoint InSize, FIntPoint& OutPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	bool TryAddItem(const FItemInstance& InItem, int32& OutRemainder);

	bool TryAddItemAt(const FItemInstance& InItem, FIntPoint InPosition);

	bool MoveItem(FGuid InInstanceId, FIntPoint InNewPosition);

	bool RemoveItem(FGuid InInstanceId, FItemInstance& OutRemoved);

	bool RemoveStackCount(FGuid InInstanceId, int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearAll();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugPrintGrid() const;

	UFUNCTION(BlueprintCallable, Exec, Category = "Inventory|Debug")
	void DebugAddItem(FName InRowName, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugMoveFirstItem(FIntPoint InNewPosition);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugRemoveFirstItem();

protected:
	virtual void BeginPlay() override;

private:
	void RebuildCache();

	void MarkCells(int32 InEntryIndex, FIntPoint InPosition, FIntPoint InSize);

	int32 CoordToIndex(FIntPoint InCoord) const { return InCoord.Y * Width + InCoord.X; }

	bool GetEntrySize(const FItemInstance& InItem, FIntPoint& OutSize) const;

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Width = 6;

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Height = 5;

	UPROPERTY(EditAnywhere, Category = "Inventory|Debug")
	TObjectPtr<UDataTable> DebugItemTable;

	UPROPERTY()
	TArray<FInventoryEntry> Entries;

	TArray<int32> CellCache;
};