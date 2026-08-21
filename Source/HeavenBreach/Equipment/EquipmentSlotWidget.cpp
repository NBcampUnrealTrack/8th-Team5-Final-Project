// Fill out your copyright notice in the Description page of Project Settings.

#include "EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EquipmentComponent.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "Blueprint/WidgetTree.h"

void UEquipmentSlotWidget::InitSlot(EEquipSlotType InSlotType, UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	SlotType = InSlotType;
	BoundEquipment = InEquipment;
	BoundInventory = InInventory;
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (BoundEquipment && BoundEquipment->IsSlotOccupied(SlotType))
		{
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
	}

	return FReply::Unhandled();
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (BoundEquipment == nullptr || BoundInventory == nullptr)
		{
			return FReply::Unhandled();
		}

		FItemInstance EquippedItem;
		if (!BoundEquipment->GetItemInSlot(SlotType, EquippedItem))
		{
			return FReply::Unhandled();
		}

		FItemViewData ViewData;
		if (!ResolveItemViewData(EquippedItem.ItemRow, ViewData)) // 사용자 데이터 해동 함수
		{
			return FReply::Unhandled();
		}

		FIntPoint FreePosition;
		if (BoundInventory->FindFirstFreeSlot(GetOccupiedSize(EquippedItem, ViewData), FreePosition))
		{
			BoundInventory->Server_UnequipItem(BoundEquipment, SlotType, FreePosition);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (BoundEquipment == nullptr) return;

	FItemInstance EquippedItem;
	if (!BoundEquipment->GetItemInSlot(SlotType, EquippedItem)) return;

	FItemViewData ViewData;
	if (!ResolveItemViewData(EquippedItem.ItemRow, ViewData)) return;

	// 드래그 비주얼 생성
	UImage* DragVisual = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath))
	{
		DragVisual->SetBrushFromTexture(IconTexture, false);
	}
	FVector2D SlotSize = InGeometry.GetLocalSize();
	DragVisual->SetDesiredSizeOverride(SlotSize);
	DragVisual->SetOpacity(0.7f);

	// DragDropOperation 설정
	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = EquippedItem.InstanceId;
	Operation->GrabOffset = FIntPoint::ZeroValue;
	Operation->GrabPixelOffset = SlotSize * 0.5f;
	Operation->SourceInventory = nullptr;
	Operation->SourceEquipment = BoundEquipment;
	Operation->SourceSlot = SlotType; // 자기 자신의 SlotType 전달!
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::MouseDown;

	OutOperation = Operation;
}

// 4. 아이템을 이 슬롯 위에 놓았을 때 (Drop)
bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr || BoundEquipment == nullptr)
	{
		return false;
	}

	// 인벤토리에서 이 장비 슬롯으로 드래그해 온 경우
	if (Operation->SourceInventory != nullptr)
	{
		Operation->SourceInventory->Server_EquipItem(BoundEquipment, Operation->InstanceId, SlotType);
		return true;
	}

	return false;
}

void UEquipmentSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// WBP 디자이너 화면에서 EmptySlotIcon을 지정했을 때 실시간으로 미리보기 제공
	if (IconImage && EmptySlotIcon)
	{
		IconImage->SetBrushFromTexture(EmptySlotIcon, false);
		IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyIconOpacity));
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (SlotLabel) SlotLabel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UEquipmentSlotWidget::UpdateSlot(bool bHasItem, const FString& DisplayName, UTexture2D* IconTexture)
{
	if (bHasItem)
	{
		// 1. 아이템이 장착된 경우
		if (IconImage)
		{
			if (IconTexture)
			{
				IconImage->SetBrushFromTexture(IconTexture, false);
				IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (SlotLabel)
		{
			if (IconTexture)
			{
				SlotLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
			else
			{
				SlotLabel->SetText(FText::FromString(DisplayName));
				SlotLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
	else
	{
		// 2. 아이템이 해제된 경우 (빈 슬롯)
		if (EmptySlotIcon && IconImage)
		{
			// 빈 슬롯 실루엣 아이콘이 등록되어 있다면 반투명하게 표시
			IconImage->SetBrushFromTexture(EmptySlotIcon, false);
			IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyIconOpacity));
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (SlotLabel)
			{
				SlotLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			// 실루엣 아이콘이 없다면 기존 아이콘 이미지를 확실히 숨김 (★ 잔상 해결 핵심)
			if (IconImage)
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}

			if (SlotLabel)
			{
				SlotLabel->SetText(FText::FromString(DisplayName));
				SlotLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
}

