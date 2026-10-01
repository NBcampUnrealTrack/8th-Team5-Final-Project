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
class UItemInfoPopupWidget;
class UInventoryWidget;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	float CellSize = 70.0f;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> BoundEquipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	UPROPERTY()
	TObjectPtr<UInventoryWidget> OwningInventoryWidget;

	// 장착 슬롯에 마우스를 올렸을 때 띄울 아이템 정보 팝업 클래스. InventoryWidget의
	// ItemInfoPopupClass와 동일하게, 이 슬롯이 속한 WBP(예: WBP_EquipmentSlot)의
	// Class Defaults에서 WBP_ItemInfoPopup으로 지정해줘야 실제로 뜬다 - 비워두면
	// UpdateHoverInfoPopup이 아무 것도 하지 않고 조용히 빠져나간다.
	UPROPERTY(EditDefaultsOnly, Category = "Item Info")
	TSubclassOf<UItemInfoPopupWidget> ItemInfoPopupClass;

	void InitSlot(EEquipSlotType InSlotType, UEquipmentComponent* InEquipment, UInventoryComponent* InInventory, UInventoryWidget* InOwningInventoryWidget = nullptr);
	void UpdateSlot(bool bHasItem, const FString& DisplayName, UTexture2D* IconTexture);

protected:

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Durability;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Broken;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_BrokenBG;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_GradeBG;

	// 양손무기(활/지팡이) 장착 중이라 보조 장비 슬롯 자체를 쓸 수 없을 때 켜지는 배경.
	// Image_BrokenBG와 같은 자리(Overlay에서 IconImage보다 아래)에 두면 된다.
	// UMG에서 같은 이름의 Image를 추가하면 자동 바인딩된다(안 만들어도 무방 - nullptr).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_LockedBG;

	// 잠금 상태에서 보여줄 텍스트(예: "양손무기 장착 중"). Text_Broken과 같은 패턴.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Locked;

	// 잠금 상태 중앙에 띄우는 자물쇠 아이콘. Image_LockedBG와 달리 이건 맨 위에 그려져야
	// 보이므로, UMG Overlay에서 IconImage(Scale Box)보다 뒤(아래) 순서에 둬야 한다 -
	// 즉 계층 트리에서 제일 마지막 자식으로 넣는다. 같은 이름의 Image를 추가하면 자동 바인딩된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_LockedIcon;

	// UpdateDurabilityBar에서 계산해서 UpdateSlot의 아이콘 틴트에서 참조한다.
	bool bIsItemBroken = false;

	void UpdateDurabilityBar(bool bHasItem);
	void UpdateGradeBackground(bool bHasItem);
	void UpdateLockedOverlay(bool bHasItem);

	// 인벤토리 그리드 칸의 UpdateHoverInfoPopup(InventoryWidget)과 동일한 패턴 -
	// 슬롯이 하나뿐이라 셀 좌표 변환은 필요 없고, 지금 슬롯에 낀 아이템만 보면 된다.
	UPROPERTY()
	TObjectPtr<UItemInfoPopupWidget> HoverInfoPopup;

	FGuid HoveredItemInstanceId;

	void UpdateHoverInfoPopup();
	void ClearHoverInfoPopup();
};