#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OpenedBagWindowWidget.generated.h"

class UInventoryWidget;
class UInventoryComponent;
class UEquipmentComponent;
class UTextBlock;
class UImage;
class UButton;

UCLASS()
class HEAVENBREACH_API UOpenedBagWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UEquipmentComponent* InEquipment, UInventoryComponent* InOwningInventory);

	void PlaceNear(FVector2D InViewportPosition);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Bag")
	void CloseWindow();

	void SetOpener(UInventoryWidget* InOpener);

	UInventoryWidget* GetOpener() const { return Opener.Get(); }

	void CloseFromUser();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UInventoryWidget> BagPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TitleBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:
	UFUNCTION()
	void HandleOpenedBagChanged();

	UFUNCTION()
	void HandleCloseClicked();

	void Refresh();

	void SetWindowPosition(FVector2D InPosition);

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> Equipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> OwningInventory;

	TWeakObjectPtr<UInventoryWidget> Opener;

	FVector2D WindowPosition = FVector2D::ZeroVector;
	FVector2D DragStartMouse = FVector2D::ZeroVector;
	FVector2D DragStartWindow = FVector2D::ZeroVector;
	bool bDraggingWindow = false;
};
