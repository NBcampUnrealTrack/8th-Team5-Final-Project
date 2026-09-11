#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryCellWidget.generated.h"

UENUM(BlueprintType)
enum class EInventoryCellVisualState : uint8
{
	Default,
	Hovered,
	PlacementValid,
	PlacementInvalid
};

UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UInventoryCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Cell")
	void InitializeCell(FIntPoint InCellPosition);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Cell")
	void SetVisualState(EInventoryCellVisualState InState);

	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	FIntPoint GetCellPosition() const { return CellPosition; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	EInventoryCellVisualState GetVisualState() const { return VisualState; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Cell", meta = (DisplayName = "On Visual State Changed"))
	void BP_OnVisualStateChanged(EInventoryCellVisualState NewState);

private:
	UPROPERTY(VisibleAnywhere, Category = "Inventory|Cell")
	FIntPoint CellPosition = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Inventory|Cell")
	EInventoryCellVisualState VisualState = EInventoryCellVisualState::Default;
};
