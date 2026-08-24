#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTypes.h"
#include "InventoryItemWidget.generated.h"

class UInventoryComponent;

/**
 * Per-item input layer used to start inventory drags from the item's geometry
 * instead of from the geometry of the entire inventory widget.
 */
UCLASS()
class HEAVENBREACH_API UInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeItem(
		UInventoryComponent* InSourceInventory,
		const FItemInstance& InItem,
		FIntPoint InItemSize,
		float InCellSize);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;

private:
	UPROPERTY()
	TObjectPtr<UInventoryComponent> SourceInventory;

	FItemInstance Item;
	FIntPoint ItemSize = FIntPoint(1, 1);
	FVector2D GrabPixelOffset = FVector2D::ZeroVector;
	float CellSize = 64.0f;
	bool bSplitRequested = false;
};
