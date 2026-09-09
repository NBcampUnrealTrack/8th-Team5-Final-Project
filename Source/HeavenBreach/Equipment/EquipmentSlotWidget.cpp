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
		if (!ResolveItemViewData(EquippedItem.ItemRow, ViewData))
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

	const FIntPoint ItemGridSize = GetOccupiedSize(EquippedItem, ViewData);
	const FVector2D VisualSize(ItemGridSize.X * CellSize, ItemGridSize.Y * CellSize);
	UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = EquippedItem.InstanceId;
	Operation->GrabOffset = FIntPoint::ZeroValue;
	Operation->GrabPixelOffset = VisualSize * 0.5f;
	Operation->SourceInventory = nullptr;
	Operation->SourceEquipment = BoundEquipment;
	Operation->SourceSlot = SlotType;
	Operation->bRotated = EquippedItem.bRotated;
	Operation->Pivot = EDragPivot::MouseDown;

	// 인벤토리 칸에서 드래그할 때와 똑같이, Operation이 자기만의 Slate 비주얼
	// (SBox+SImage)을 직접 뷰포트에 붙여서 관리하게 한다. 예전 방식(DefaultDragVisual에
	// 단순 UImage)은 크기가 슬롯 지오메트리 그대로라 작게 나오고, T키 회전도
	// 전혀 반영이 안 됐다.
	Operation->CreateVisual(this, IconTexture, VisualSize);

	OutOperation = Operation;
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr)
	{
		return false;
	}

	if (BoundEquipment == nullptr)
	{
		return false;
	}

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

	if (IconImage && EmptySlotIcon)
	{
		IconImage->SetBrushFromTexture(EmptySlotIcon, false);
		IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyIconOpacity));
		IconImage->SetRenderScale(EmptyIconScale);
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
				IconImage->SetRenderScale(FVector2D(1.0f, 1.0f));
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
			IconImage->SetBrushFromTexture(EmptySlotIcon, false);
			IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, EmptyIconOpacity));
			IconImage->SetRenderScale(EmptyIconScale);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (SlotLabel)
			{
				SlotLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
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