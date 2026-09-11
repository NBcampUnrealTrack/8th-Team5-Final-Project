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
class UProgressBar;
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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Durability;

	// 장착된 아이템 파손 시에만 보이는 텍스트. UMG에서 위젯을 지워서 지금은 안 씀 ?
	// 바인딩은 남겨둬도 무해하다(매칭되는 위젯 없으면 nullptr).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Broken;

	// 파손 시 아이콘 뒤에 깔리는 반투명 빨간 배경. UMG에 같은 이름의 Image를
	// 추가하면 자동 바인딩된다(없어도 에러 안 남).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_BrokenBG;

	// 장착된 아이템 등급을 색으로 보여주는 배경. Overlay에서 Image_BrokenBG보다
	// 아래(먼저)에 있어야 한다. UMG에 같은 이름의 Image를 추가하면 자동 바인딩된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_GradeBG;

	// UpdateDurabilityBar에서 계산해서 UpdateSlot의 아이콘 틴트에서도 재사용한다.
	bool bIsItemBroken = false;

	void UpdateDurabilityBar(bool bHasItem);
	void UpdateGradeBackground(bool bHasItem);
};