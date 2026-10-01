#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/HB_ActivatableWidget.h"
#include "Inventory/InventoryTypes.h"
#include "InventoryWidget.generated.h"

class UCanvasPanel;
class UInventoryComponent;
class UEquipmentComponent;
class UEquipmentWidget;
class UItemActionMenuWidget;
class UItemSplitPopupWidget;
class UItemInfoPopupWidget;
class UInventoryCellWidget;
class UInventoryItemWidget;
class UInventoryDragDropOperation;
class UShopComponent;
struct FItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScrollChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemActivated, FGuid, InstanceId);

/**
 *
 *
 */
UCLASS()
class HEAVENBREACH_API UInventoryWidget : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget();
	virtual void SynchronizeProperties() override;

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

	bool ShouldSuppressDropOnCancel() const { return bSuppressDropOnCancel; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool BP_ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const { return ScreenToCell(InScreenPosition, OutCell); }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetParentContainerWidget(UUserWidget* InParentWidget);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetEquipmentPanelVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetShopComponent(UShopComponent* InShop);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ScrollRows(int32 InDelta);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetScrollRatio() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetScrollByRatio(float InRatio);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsScrollable() const;

	/** Cleans transient drag and popup state before an embedded HUD inventory is hidden. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void PrepareForHide();
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float CellSize = 64.0f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 VisibleRows = 0;

#if WITH_EDITORONLY_DATA
#pragma region InventoryDesignerPreview
	/** Show empty cells in the widget designer without binding a gameplay inventory. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Designer Preview")
	bool bShowDesignerGrid = true;

	UPROPERTY(EditAnywhere, Category = "Inventory|Designer Preview", meta = (ClampMin = "1", ClampMax = "32", EditCondition = "bShowDesignerGrid"))
	int32 PreviewColumns = 6;

	/** Total preview rows. VisibleRows still limits the rows displayed, as it does at runtime. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Designer Preview", meta = (ClampMin = "1", ClampMax = "32", EditCondition = "bShowDesignerGrid"))
	int32 PreviewRows = 5;
#pragma endregion
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Widget")
	TSubclassOf<UInventoryCellWidget> InventoryCellWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Widget")
	TSubclassOf<UInventoryItemWidget> InventoryItemWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEquipmentWidget> EquipmentPanel;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnScrollChanged OnScrollChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryItemActivated OnItemActivated;

private:
#if WITH_EDITOR
	void RefreshDesignerGrid();
#endif
	void BuildBackground();
	void BuildItems();
	bool ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const;
	bool ScreenToLocal(const FVector2D& InScreenPosition, FVector2D& OutLocal) const;
	bool HandleRightClick(FIntPoint InCell);
	bool HandleContextMenu(FIntPoint InCell);
	bool HandleDoubleClick(FIntPoint InCell);
	bool HandleQuickSlotRegister(int32 InSlotIndex);
	void RefocusAfterDrop();
	void TryReclaimFocus();
	bool ShouldReclaimFocus() const;
	void SetHoveredCell(FIntPoint InCell);
	void ClearHoveredCell();
	bool UpdatePlacementPreview(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation);
	bool UpdatePlacementPreviewAt(const FVector2D& InScreenSpacePosition, UDragDropOperation* InOperation);
	void ClearPlacementPreview();

	int32 GetVisibleRowCount() const;
	int32 GetMaxScrollRow() const;
	int32 ScrollOffsetRow = 0;

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
	bool bHasValidPress = false;

	bool bSuppressDropOnCancel = false;

	void ShowSplitPopupForDrop(FGuid InInstanceId, FIntPoint InTargetCell, FVector2D InScreenPosition);

	UPROPERTY(EditDefaultsOnly, Category = "Context Menu")
	TSubclassOf<UItemActionMenuWidget> ItemContextMenuClass;

	UPROPERTY()
	TObjectPtr<UItemActionMenuWidget> ActiveContextMenu;

	UPROPERTY(EditDefaultsOnly, Category = "Context Menu")
	TSubclassOf<UItemSplitPopupWidget> ItemSplitPopupClass;

	UPROPERTY()
	TObjectPtr<UItemSplitPopupWidget> ActiveSplitPopup;

	UPROPERTY(EditDefaultsOnly, Category = "Context Menu")
	TSubclassOf<UItemInfoPopupWidget> ItemInfoPopupClass;

	UPROPERTY()
	TObjectPtr<UItemInfoPopupWidget> ActiveInfoPopup;
	FVector2D PressedScreenPosition = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UItemInfoPopupWidget> HoverInfoPopup;

	FGuid HoveredItemInstanceId;

	void UpdateHoverInfoPopup(FIntPoint InCell);
	void ClearHoverInfoPopup();

	// BuildStatTextForItem은 EquipmentSlotWidget과 공유하기 위해 InventoryTypes.h/.cpp의
	// 공용 함수로 옮겼다 (이전엔 여기 private 멤버였음).

	UPROPERTY()
	TObjectPtr<UUserWidget> ParentContainerWidget;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryCellWidget>> CellWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryCellWidget>> PlacementPreviewWidgets;

	FIntPoint HoveredCell = FIntPoint(-1, -1);
	TObjectPtr<UShopComponent> LinkedShop;

	TWeakObjectPtr<UInventoryDragDropOperation> ActiveDragOperation;
};