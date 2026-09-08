#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/HB_ActivatableWidget.h"
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
class UShopComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScrollChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemActivated, FGuid, InstanceId);

/**
 * 인벤토리 화면.
 *
 * UI.Layer.Menu 스택에 push 되어 동작한다. 활성 상태에서 Menu 입력 모드를
 * 선언하므로 PlayerController가 이동/공격 입력을 따로 막을 필요가 없다.
 *
 * 상자/상점 UI 안에 자식 위젯으로 들어가는 경우도 있는데, 그때는 활성화되지
 * 않으므로 입력 모드나 Back 처리에 관여하지 않는다.
 */
UCLASS()
class HEAVENBREACH_API UInventoryWidget : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget();

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

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ScrollRows(int32 InDelta);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetScrollRatio() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetScrollByRatio(float InRatio);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsScrollable() const;
protected:
	virtual void NativeDestruct() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
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
	void BuildBackground();
	void BuildItems();
	bool ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const;
	bool ScreenToLocal(const FVector2D& InScreenPosition, FVector2D& OutLocal) const;
	bool HandleRightClick(FIntPoint InCell);
	bool HandleContextMenu(FIntPoint InCell);
	bool HandleDoubleClick(FIntPoint InCell);
	bool HandleQuickSlotRegister(int32 InSlotIndex);
	void RefocusAfterDrop();
	void SetHoveredCell(FIntPoint InCell);
	void ClearHoveredCell();
	bool UpdatePlacementPreview(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation);
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
	// 우클릭이 실제로 유효한 아이템 칸에서 시작됐는지 추적. 이게 없으면 인벤토리 바깥이나
	// 빈 칸에서 우클릭해도 이전에 눌렀던 PressedCell/bPressedWithCtrl 값이 그대로 남아있어서,
	// Ctrl 없이 아무데나 우클릭해도 예전 아이템의 컨텍스트 메뉴가 다시 뜨는 버그가 있었음.
	bool bHasValidPress = false;

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
	FSlateBrush DragVisualBrush;
	TSharedPtr<class SWidget> DragVisualRoot;
	FVector2D PressedScreenPosition = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UUserWidget> ParentContainerWidget;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryCellWidget>> CellWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryCellWidget>> PlacementPreviewWidgets;

	FIntPoint HoveredCell = FIntPoint(-1, -1);
	TObjectPtr<UShopComponent> LinkedShop;
};
