#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UCanvasPanel;
class UInventoryComponent;
class UEquipmentComponent;
class UEquipmentWidget;
class UItemActionMenuWidget;
class UItemSplitPopupWidget;
class UInventoryCellWidget;
class UShopComponent;

UCLASS()
class HEAVENBREACH_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void BindToInventory(UInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshGrid();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void BindToEquipment(UEquipmentComponent* InEquipment);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetOwningInventory(UInventoryComponent* InOwningInventory);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetLinkedContainer(UInventoryComponent* InContainer);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetLinkedEquipment(UEquipmentComponent* InEquipment);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetBoundInventory() const { return BoundInventory; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool BP_ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const { return ScreenToCell(InScreenPosition, OutCell); }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetParentContainerWidget(UUserWidget* InParentWidget);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetEquipmentPanelVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetShopComponent(UShopComponent* InShop);

protected:
	virtual void NativeDestruct() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float CellSize = 64.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Widget")
	TSubclassOf<UInventoryCellWidget> InventoryCellWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEquipmentWidget> EquipmentPanel;

private:
	void BuildBackground();
	void BuildItems();
	bool ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const;
	bool ScreenToLocal(const FVector2D& InScreenPosition, FVector2D& OutLocal) const;
	bool HandleRightClick(FIntPoint InCell);
	bool HandleContextMenu(FIntPoint InCell);
	bool HandleDoubleClick(FIntPoint InCell);
	void RefocusAfterDrop();
	void SetHoveredCell(FIntPoint InCell);
	void ClearHoveredCell();

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> OwningInventory;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> LinkedContainer;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> LinkedEquipment;

	FIntPoint PressedCell = FIntPoint::ZeroValue;
	FVector2D PressedLocalPosition = FVector2D::ZeroVector;
	bool bPressedWithCtrl = false;

	void ShowSplitPopupForDrop(FGuid InInstanceId, FIntPoint InTargetCell, FVector2D InScreenPosition);

	UPROPERTY(EditDefaultsOnly, Category = "Context Menu")
	TSubclassOf<UItemActionMenuWidget> ItemContextMenuClass;

	UPROPERTY()
	TObjectPtr<UItemActionMenuWidget> ActiveContextMenu;

	UPROPERTY(EditDefaultsOnly, Category = "Context Menu")
	TSubclassOf<UItemSplitPopupWidget> ItemSplitPopupClass;

	UPROPERTY()
	TObjectPtr<UItemSplitPopupWidget> ActiveSplitPopup;

	FVector2D PressedScreenPosition = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UUserWidget> ParentContainerWidget;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryCellWidget>> CellWidgets;

	FIntPoint HoveredCell = FIntPoint(-1, -1);
	TObjectPtr<UShopComponent> LinkedShop;
};
