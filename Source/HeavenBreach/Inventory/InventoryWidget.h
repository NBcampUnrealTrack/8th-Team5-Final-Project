#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UCanvasPanel;
class UInventoryComponent;
class UEquipmentComponent;
class UEquipmentWidget;

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

protected:
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float CellSize = 64.0f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEquipmentWidget> EquipmentPanel;

private:
	void BuildBackground();

	void BuildItems();

	bool ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const;

	bool ScreenToLocal(const FVector2D& InScreenPosition, FVector2D& OutLocal) const;

	bool HandleRightClick(FIntPoint InCell);

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
};