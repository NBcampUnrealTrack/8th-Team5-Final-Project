#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTypes.h"
#include "EquipmentWidget.generated.h"

class UCanvasPanel;
class UEquipmentComponent;
class UInventoryComponent;

UCLASS()
class HEAVENBREACH_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void BindToEquipment(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RefreshSlots();

protected:
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION()
	void HandleEquipmentChanged();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> SlotCanvas;

	UPROPERTY(EditAnywhere, Category = "Equipment")
	float SlotSize = 64.0f;

	UPROPERTY(EditAnywhere, Category = "Equipment")
	float SlotGap = 8.0f;

private:
	FVector2D GetSlotPosition(EEquipSlotType InSlot) const;

	bool ScreenToSlot(const FVector2D& InScreenPosition, EEquipSlotType& OutSlot) const;

	void BuildSlots();

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> BoundEquipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	EEquipSlotType PressedSlot = EEquipSlotType::None;
};