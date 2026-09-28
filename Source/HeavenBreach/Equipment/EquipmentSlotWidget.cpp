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

	// �κ��丮 ĭ���� �巡���� ���� �Ȱ���, Operation�� �ڱ⸸�� Slate ���־�
	// (SBox+SImage)�� ���� ����Ʈ�� �ٿ��� �����ϰ� �Ѵ�. ���� ���(DefaultDragVisual��
	// �ܼ� UImage)�� ũ�Ⱑ ���� ������Ʈ�� �״�ζ� �۰� ������, TŰ ȸ����
	// ���� �ݿ��� �� �ƴ�.
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
	UpdateLockedOverlay(bHasItem);

	if (bHasItem)
	{
		// 1. �������� ������ ���
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
		// 2. �������� ������ ��� (�� ����)
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

	// Text_Broken/Image_BrokenBG�� ProgressBar_Durability ������ �����ϰ� �׻� �����Ѵ�.
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

	// 100%(�� ��)�� ��ҿ� ����ϰ� �����, ��� �����ϸ� �׶����� �����ش�.
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

	// InventoryTypes.h ���� �Լ��� �κ��丮 ĭ�̶� ���� �����(RGB�� ����, ���ĸ�
	// ���� ũ�⿡ �°� ���⼭ ���� ����). Normal�� ���ó�� �� ���̰� �д�.
	FLinearColor GradeColor = GetGradeTopColor(ViewData.Grade);
	GradeColor.A = (ViewData.Grade == EItemGrade::Normal) ? 0.0f : 0.35f;

	Image_GradeBG->SetColorAndOpacity(GradeColor);
	Image_GradeBG->SetVisibility(GradeColor.A > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEquipmentSlotWidget::UpdateLockedOverlay(bool bHasItem)
{
	// 슬롯이 비어있는데(bHasItem == false), 주무기로 활/지팡이가 꽂혀 있어서
	// 애초에 아무것도 낄 수 없는 상태일 때만 켠다. 뭔가 장착돼 있으면(정상 상황이라면
	// CanEquip이 막아서 발생하지 않는다) 그쪽 표시가 우선이라 잠금 오버레이는 끈다.
	const bool bLocked = !bHasItem
		&& SlotType == EEquipSlotType::SecondaryEquip
		&& BoundEquipment != nullptr
		&& BoundEquipment->IsSecondarySlotLockedByWeapon();

	if (Text_Locked != nullptr)
	{
		Text_Locked->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_LockedBG != nullptr)
	{
		Image_LockedBG->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_LockedIcon != nullptr)
	{
		Image_LockedIcon->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}