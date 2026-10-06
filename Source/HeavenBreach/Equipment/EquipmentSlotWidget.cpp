// Fill out your copyright notice in the Description page of Project Settings.

#include "EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "EquipmentComponent.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Item/ItemInfoPopupWidget.h"
#include "Item/ItemStatBinding.h"
#include "InventoryWidget.h"
#include "TimerManager.h"

void UEquipmentSlotWidget::InitSlot(EEquipSlotType InSlotType, UEquipmentComponent* InEquipment, UInventoryComponent* InInventory, UInventoryWidget* InOwningInventoryWidget)
{
	SlotType = InSlotType;
	BoundEquipment = InEquipment;
	BoundInventory = InInventory;
	OwningInventoryWidget = InOwningInventoryWidget;
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (InMouseEvent.IsControlDown() && OwningInventoryWidget != nullptr)
		{
			return FReply::Handled().SetUserFocus(OwningInventoryWidget->TakeWidget(), EFocusCause::Mouse);
		}
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

		if (InMouseEvent.IsControlDown())
		{
			if (OwningInventoryWidget != nullptr && BoundEquipment->IsSlotOccupied(SlotType))
			{
				ClearHoverInfoPopup();
				if (UWorld* World = GetWorld())
				{
					TWeakObjectPtr<UInventoryWidget> WeakOwner = OwningInventoryWidget;
					TWeakObjectPtr<UEquipmentComponent> WeakEquipment = BoundEquipment;
					const EEquipSlotType MenuSlot = SlotType;
					World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakOwner, WeakEquipment, MenuSlot]()
						{
							if (WeakOwner.IsValid() && WeakEquipment.IsValid())
							{
								WeakOwner->OpenEquippedItemMenu(WeakEquipment.Get(), MenuSlot);
							}
						}));
				}
			}
			return FReply::Handled();
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

FReply UEquipmentSlotWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UpdateHoverInfoPopup();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	ClearHoverInfoPopup();
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	// 드래그를 시작하면(아이템을 집어드는 순간) 호버 툴팁은 치워야 한다 - 안 그러면
	// 드래그 중에도 예전 위치에 붙박여서 남아있는다.
	ClearHoverInfoPopup();

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

void UEquipmentSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const bool bSuppressed = OwningInventoryWidget != nullptr && OwningInventoryWidget->ShouldSuppressDropOnCancel();

	if (!bSuppressed)
	{
		if (const UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation))
		{
			if (Operation->SourceEquipment == BoundEquipment
				&& Operation->SourceSlot == SlotType
				&& BoundEquipment != nullptr
				&& BoundInventory != nullptr)
			{
				BoundInventory->Server_DropEquippedItem(BoundEquipment, SlotType);
			}
		}
	}

	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UEquipmentSlotWidget::NativeDestruct()
{
	ClearHoverInfoPopup();
	Super::NativeDestruct();
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

void UEquipmentSlotWidget::UpdateHoverInfoPopup()
{
	// InventoryWidget::UpdateHoverInfoPopup과 같은 패턴 - 그리드 셀 대신 이 슬롯 하나만 본다.
	if (BoundEquipment == nullptr || ItemInfoPopupClass == nullptr)
	{
		return;
	}

	FItemInstance EquippedItem;
	if (!BoundEquipment->GetItemInSlot(SlotType, EquippedItem))
	{
		ClearHoverInfoPopup();
		return;
	}

	// 같은 아이템 위에서 계속 움직이는 중이면 다시 만들지 않고 위치만 따라가게 한다.
	if (HoverInfoPopup != nullptr && HoveredItemInstanceId == EquippedItem.InstanceId)
	{
		const FVector2D InfoViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(16.f, 16.f);
		HoverInfoPopup->SetPositionInViewport(InfoViewportPosition, false);
		return;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(EquippedItem.ItemRow, ViewData))
	{
		ClearHoverInfoPopup();
		return;
	}

	if (HoverInfoPopup != nullptr)
	{
		HoverInfoPopup->RemoveFromParent();
		HoverInfoPopup = nullptr;
	}

	HoverInfoPopup = CreateWidget<UItemInfoPopupWidget>(GetOwningPlayer(), ItemInfoPopupClass);
	if (HoverInfoPopup == nullptr)
	{
		HoveredItemInstanceId = FGuid();
		return;
	}

	UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);

	const FString StatText = BuildStatTextForItem(EquippedItem);
	const FString EquipSlotLabel = GetEquipSlotDisplayName(ViewData.EquipSlotType);

	// InventoryWidget::UpdateHoverInfoPopup과 동일 - 옛 BenefitDescription / RiskDescription 문자열은
	// 더 안 읽는다. 옵션 배열에서 문장을 만든다. 각인도 같은 함수가 붙인다.
	const FString BenefitLines = BuildBenefitLinesForItem(EquippedItem, ViewData);
	const FString RiskLines = BuildRiskLinesForItem(EquippedItem, ViewData);

	HoverInfoPopup->SetupInfo(ViewData.DisplayName, ViewData.Grade, ViewData.Description,
		BenefitLines, RiskLines,
		ViewData.SellValue, IconTexture, StatText, /*bInCompact=*/false,
		EquipSlotLabel, EquippedItem.CurrentDurability, GetMaxDurabilityForGrade(ViewData.Grade));

	HoverInfoPopup->AddToViewport(100);
	// InventoryWidget 쪽과 동일한 이유 - 포커스를 가져가면 안 되고(마우스 이동만으로
	// 키/드래그 입력이 끊김), HitTestInvisible이라야 마우스 이벤트가 팝업을 뚫고
	// 슬롯 자체로 그대로 전달된다.
	HoverInfoPopup->SetVisibility(ESlateVisibility::HitTestInvisible);

	const FVector2D InfoViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(16.f, 16.f);
	HoverInfoPopup->SetPositionInViewport(InfoViewportPosition, false);

	HoveredItemInstanceId = EquippedItem.InstanceId;
}

void UEquipmentSlotWidget::ClearHoverInfoPopup()
{
	if (HoverInfoPopup != nullptr)
	{
		HoverInfoPopup->RemoveFromParent();
		HoverInfoPopup = nullptr;
	}
	HoveredItemInstanceId = FGuid();
}