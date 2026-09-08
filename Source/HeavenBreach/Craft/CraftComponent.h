#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftTypes.h"
#include "InventoryTypes.h"
#include "CraftComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCraftSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraftCompleted, bool, bSuccess);

UCLASS(ClassGroup = (Craft), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UCraftComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftComponent();

	static constexpr int32 NumCraftSlots = 5;

	void SetGoldItemRow(const FDataTableRowHandle& InRow) { GoldItemRow = InRow; }

	UFUNCTION(BlueprintPure, Category = "Craft")
	int32 GetCurrentGold() const;

	UFUNCTION(BlueprintCallable, Category = "Craft")
	bool GetPreviewResult(FDataTableRowHandle& OutItemRow, int32& OutCount, int32& OutGoldCost, bool& bOutCanAfford) const;

	UPROPERTY(BlueprintAssignable, Category = "Craft")
	FOnCraftSlotsChanged OnCraftSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Craft")
	FOnCraftCompleted OnCraftCompleted;

	void SetTargetStash(UInventoryComponent* InStash);

	UFUNCTION(BlueprintPure, Category = "Craft")
	const TArray<FItemInstance>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Craft")
	ECraftMode GetCraftMode() const { return CraftMode; }

	UFUNCTION(BlueprintCallable, Category = "Craft")
	bool CanCraft() const;

	bool FindMatchingRecipe(FCraftRecipeRow& OutRecipe) const;

	UFUNCTION(Server, Reliable)
	void Server_SetCraftMode(ECraftMode InMode);

	UFUNCTION(Server, Reliable)
	void Server_PlaceItem(FGuid InInstanceId, int32 InSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(int32 InSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_ExecuteCraft();

	UFUNCTION(Server, Reliable)
	void Server_ReturnAllToStash();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetRecipeTable(UDataTable* InTable) { RecipeTable = InTable; }

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void PlaceFromStash(FGuid InInstanceId, int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void RemoveFromSlot(int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void ExecuteCraft();

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void ReturnAllToStash();

	UFUNCTION(BlueprintPure, Category = "Craft")
	static bool GetItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void PlaceToFirstEmptySlot(FGuid InInstanceId);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Craft")
	TObjectPtr<UDataTable> RecipeTable;

	UFUNCTION()
	void OnRep_Slots();

	UPROPERTY()
	FDataTableRowHandle GoldItemRow;

private:
	bool ReturnSlotToStash(int32 InSlotIndex);
	void BuildSlotCountMap(TMap<FName, int32>& OutCounts) const;

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FItemInstance> Slots;

	UPROPERTY(Replicated)
	ECraftMode CraftMode = ECraftMode::Craft;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> TargetStash;
};