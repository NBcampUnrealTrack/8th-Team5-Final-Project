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
	float EmptyIconOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Style")
	FVector2D EmptyIconScale = FVector2D(0.8f, 0.8f);

	// 인벤토리 그리드의 CellSize와 반드시 같은 값으로 맞춰야 한다. 장비창에서
	// 드래그를 시작할 때 아이템의 실제 그리드 칸 크기(가로/세로 칸 수 × CellSize)로
	// 드래그 비주얼을 만들기 위해 필요하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	float CellSize = 70.0f;

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
