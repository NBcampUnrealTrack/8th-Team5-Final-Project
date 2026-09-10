// Fill out your copyright notice in the Description page of Project Settings.

#include "EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
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
	UpdateDurabilityBar(bHasItem);
	UpdateGradeBackground(bHasItem);

	if (bHasItem)
	{
		// 1. 아이템이 장착된 경우
		if (IconImage)
		{
			if (IconTexture)
			{
				IconImage->SetBrushFromTexture(IconTexture, false);
				IconImage->SetColorAndOpacity(bIsItemBroken
					? FLinearColor(1.0f, 0.35f, 0.35f, 1.0f)
					: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
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

void UEquipmentSlotWidget::UpdateDurabilityBar(bool bHasItem)
{
	bIsItemBroken = false;

	FItemInstance EquippedItem;
	FItemViewData ViewData;

	const bool bHasTrackedItem = bHasItem && BoundEquipment != nullptr
		&& BoundEquipment->GetItemInSlot(SlotType, EquippedItem)
		&& ResolveItemViewData(EquippedItem.ItemRow, ViewData)
		&& EquippedItem.CurrentDurability >= 0.f;

	if (bHasTrackedItem)
	{
		bIsItemBroken = (EquippedItem.CurrentDurability == 0.f);
	}

	// Text_Broken/Image_BrokenBG는 ProgressBar_Durability 유무와 무관하게 항상 갱신한다.
	if (Text_Broken != nullptr)
	{
		Text_Broken->SetVisibility(bIsItemBroken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_BrokenBG != nullptr)
	{
		Image_BrokenBG->SetVisibility(bIsItemBroken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ProgressBar_Durability == nullptr)
	{
		return;
	}

	if (!bHasTrackedItem)
	{
		ProgressBar_Durability->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const float MaxDurability = GetMaxDurabilityForGrade(ViewData.Grade);
	const float Percent = MaxDurability > 0.f
		? FMath::Clamp(EquippedItem.CurrentDurability / MaxDurability, 0.f, 1.f)
		: 0.f;

	// 100%(새 것)면 평소엔 깔끔하게 숨기고, 닳기 시작하면 그때부터 보여준다.
	ProgressBar_Durability->SetVisibility(Percent >= 1.f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ProgressBar_Durability->SetPercent(Percent);

	FLinearColor FillColor;
	if (Percent >= 0.5f)
	{
		FillColor = FLinearColor(0.37f, 0.83f, 0.37f);
	}
	else if (Percent >= 0.2f)
	{
		FillColor = FLinearColor(0.88f, 0.76f, 0.29f);
	}
	else
	{
		FillColor = FLinearColor(0.88f, 0.35f, 0.29f);
	}
	ProgressBar_Durability->SetFillColorAndOpacity(FillColor);
}

void UEquipmentSlotWidget::UpdateGradeBackground(bool bHasItem)
{
	if (Image_GradeBG == nullptr)
	{
		return;
	}

	FItemInstance EquippedItem;
	FItemViewData ViewData;

	const bool bHasTrackedItem = bHasItem && BoundEquipment != nullptr
		&& BoundEquipment->GetItemInSlot(SlotType, EquippedItem)
		&& ResolveItemViewData(EquippedItem.ItemRow, ViewData);

	if (!bHasTrackedItem)
	{
		Image_GradeBG->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// InventoryTypes.h 공용 함수로 인벤토리 칸이랑 색을 맞춘다(RGB는 공용, 알파만
	// 슬롯 크기에 맞게 여기서 따로 조절). Normal은 평소처럼 안 보이게 둔다.
	FLinearColor GradeColor = GetGradeTopColor(ViewData.Grade);
	GradeColor.A = (ViewData.Grade == EItemGrade::Normal) ? 0.0f : 0.35f;

	Image_GradeBG->SetColorAndOpacity(GradeColor);
	Image_GradeBG->SetVisibility(GradeColor.A > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}