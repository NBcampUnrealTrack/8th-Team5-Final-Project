#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsed, FName, ItemId, float, HealAmount);

class UDataTable;
class UEquipmentComponent;
class AItemBase;

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemUsed OnItemUsed;

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

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTotalGold(FName InGoldRowName) const;

	bool ConsumeGold(const FDataTableRowHandle& InGoldRow, int32 InAmount);

	bool AddGold(const FDataTableRowHandle& InGoldRow, int32 InAmount);

	bool CanPlaceAt(FIntPoint InSize, FIntPoint InPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	bool FindFirstFreeSlot(FIntPoint InSize, FIntPoint& OutPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	bool TryAddItem(const FItemInstance& InItem, int32& OutRemainder);

	bool TryAddItemAt(const FItemInstance& InItem, FIntPoint InPosition);

	bool MoveItem(FGuid InInstanceId, FIntPoint InNewPosition);

	bool RemoveItem(FGuid InInstanceId, FItemInstance& OutRemoved);

	bool RemoveStackCount(FGuid InInstanceId, int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(FGuid InInstanceId);
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ToggleFavorite(FGuid InInstanceId);

	bool TransferItemFrom(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition);

	bool TransferItemFromAuto(UInventoryComponent* InSource, FGuid InInstanceId);

	bool MergeStack(FGuid InSourceId, FGuid InTargetId);

	bool MergeStackFrom(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId);

	bool DropItem(FGuid InInstanceId);

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(FGuid InInstanceId, FIntPoint InNewPosition);

	UFUNCTION(Server, Reliable)
	void Server_TransferItem(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemAuto(UInventoryComponent* InTarget, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_EquipItem(UEquipmentComponent* InEquipment, FGuid InInstanceId, EEquipSlotType InSlot);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItem(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemFrom(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemFromAuto(UInventoryComponent* InSource, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_MoveItemIn(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InNewPosition);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItemTo(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition);

	void SetGridSize(int32 InWidth, int32 InHeight);

	void SetEntries(const TArray<FInventoryEntry>& InEntries);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(FGuid InInstanceId);
	UFUNCTION(Server, Reliable)
	void Server_ToggleFavorite(FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_SplitStack(FGuid InInstanceId, int32 InQuantity, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_MergeStack(FGuid InSourceId, FGuid InTargetId);

	UFUNCTION(Server, Reliable)
	void Server_MergeStackFrom(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(FGuid InInstanceId);


protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Entries();

private:
	void RebuildCache();

	void MarkCells(int32 InEntryIndex, FIntPoint InPosition, FIntPoint InSize);

	int32 CoordToIndex(FIntPoint InCoord) const { return InCoord.Y * Width + InCoord.X; }

	bool GetEntrySize(const FItemInstance& InItem, FIntPoint& OutSize) const;

	bool HasMergeableSpace(FName InRowName, int32 InMaxStack) const;

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Width = 6;

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Height = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")

	float ItemUseCooldown = 6.0f;

	bool bHasUsedItem = false;
	float LastItemUseTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Inventory|Debug")
	TObjectPtr<UDataTable> DebugItemTable;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop")
	TSubclassOf<AItemBase> DropItemClass;

	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FInventoryEntry> Entries;

	TArray<int32> CellCache;
};