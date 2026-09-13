#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftTypes.h"
#include "InventoryTypes.h"
#include "CraftComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCraftSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecipeSelected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraftCompleted, bool, bSuccess);

UCLASS(ClassGroup = (Craft), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UCraftComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftComponent();

	static constexpr int32 NumCraftSlots = 5;

	void SetGoldItemRow(const FDataTableRowHandle& InRow) { GoldItemRow = InRow; }
	void SetRecipeTable(UDataTable* InTable) { RecipeTable = InTable; }
	void SetTargetStash(UInventoryComponent* InStash);

	UFUNCTION(BlueprintPure, Category = "Craft")
	int32 GetCurrentGold() const;

	UFUNCTION(BlueprintPure, Category = "Craft")
	const TArray<FItemInstance>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Craft")
	ECraftMode GetCraftMode() const { return CraftMode; }

	UFUNCTION(BlueprintPure, Category = "Craft")
	FName GetSelectedRecipeRowName() const { return SelectedRecipeRowName; }

	UFUNCTION(BlueprintPure, Category = "Craft")
	bool GetSelectedRecipe(FCraftRecipeRow& OutRecipe) const;

	UFUNCTION(BlueprintPure, Category = "Craft")
	bool GetPreviewResult(FDataTableRowHandle& OutItemRow, int32& OutCount, int32& OutGoldCost, bool& bOutCanAfford) const;

	UFUNCTION(BlueprintPure, Category = "Craft")
	bool CanCraft() const;

	UFUNCTION(BlueprintPure, Category = "Craft")
	bool IsRecipeUnlocked(const FCraftRecipeRow& InRecipe) const;

	UFUNCTION(BlueprintPure, Category = "Craft")
	static bool GetItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData);

	UPROPERTY(BlueprintAssignable, Category = "Craft")
	FOnCraftSlotsChanged OnCraftSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Craft")
	FOnRecipeSelected OnRecipeSelected;

	UPROPERTY(BlueprintAssignable, Category = "Craft")
	FOnCraftCompleted OnCraftCompleted;

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void SelectRecipe(FName InRowName);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void AutoFillFromStash();

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void PlaceFromStash(FGuid InInstanceId, int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void RemoveFromSlot(int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void ExecuteCraft();

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void ReturnAllToStash();

	UFUNCTION(BlueprintCallable, Category = "Craft")
	void PlaceToFirstMatchingSlot(FGuid InInstanceId);

	UFUNCTION(BlueprintPure, Category = "Craft")
	UDataTable* GetRecipeTable() const { return RecipeTable; }

	UFUNCTION(BlueprintCallable, Category = "Craft")
	bool HasAnyFillableIngredient() const;

	UFUNCTION(Server, Reliable)
	void Server_SetCraftMode(ECraftMode InMode);

	UFUNCTION(Server, Reliable)
	void Server_SelectRecipe(FName InRowName);

	UFUNCTION(Server, Reliable)
	void Server_AutoFillFromStash();

	UFUNCTION(Server, Reliable)
	void Server_PlaceItem(FGuid InInstanceId, int32 InSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(int32 InSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_ExecuteCraft();

	UFUNCTION(Server, Reliable)
	void Server_ReturnAllToStash();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Craft")
	TObjectPtr<UDataTable> RecipeTable;

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_SelectedRecipeRowName();

	UPROPERTY()
	FDataTableRowHandle GoldItemRow;

private:
	bool ReturnSlotToStash(int32 InSlotIndex);
	bool ConsumeRecipeItemIfLimited(const FCraftRecipeRow& InRecipe);

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FItemInstance> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedRecipeRowName)
	FName SelectedRecipeRowName = NAME_None;

	UPROPERTY(Replicated)
	ECraftMode CraftMode = ECraftMode::Craft;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> TargetStash;
};