// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentComponent.h"
#include "EquipmentSlotWidget.generated.h"

/**
 * 
 */

class UImage;
class UTextBlock;
class UEquipmentComponent;
class UInventoryComponent;

UCLASS()
class HEAVENBREACH_API UEquipmentSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	EEquipSlotType SlotType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Style")
	TObjectPtr<UTexture2D> EmptySlotIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyIconOpacity = 0.3f;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> BoundEquipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	void InitSlot(EEquipSlotType InSlotType, UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);
	void UpdateSlot(bool bHasItem, const FString& DisplayName, UTexture2D* IconTexture);
	
protected:

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativePreConstruct() override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotLabel;
};
