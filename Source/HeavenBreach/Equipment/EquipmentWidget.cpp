#include "EquipmentWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EquipmentComponent.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"

namespace
{
	const TArray<EEquipSlotType> VisibleSlots = {
		EEquipSlotType::Helmet,
		EEquipSlotType::PrimaryWeapon,
		EEquipSlotType::Chest,
		EEquipSlotType::SecondaryEquip,
		EEquipSlotType::Accessory,
		EEquipSlotType::Legs
	};
}

void UEquipmentWidget::BindToEquipment(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	BoundEquipment = InEquipment;
	BoundInventory = InInventory;

	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.AddDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	RefreshSlots();
}

void UEquipmentWidget::NativeDestruct()
{
	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	Super::NativeDestruct();
}

void UEquipmentWidget::HandleEquipmentChanged()
{
	RefreshSlots();
}

FVector2D UEquipmentWidget::GetSlotPosition(EEquipSlotType InSlot) const
{
	const float Step = SlotSize + SlotGap;

	switch (InSlot)
	{
	case EEquipSlotType::Helmet:
		return FVector2D(Step, 0.0f);
	case EEquipSlotType::PrimaryWeapon:
		return FVector2D(0.0f, Step);
	case EEquipSlotType::Chest:
		return FVector2D(Step, Step);
	case EEquipSlotType::SecondaryEquip:
		return FVector2D(Step * 2.0f, Step);
	case EEquipSlotType::Accessory:
		return FVector2D(0.0f, Step * 2.0f);
	case EEquipSlotType::Legs:
		return FVector2D(Step, Step * 2.0f);
	default:
		return FVector2D::ZeroVector;
	}
}

bool UEquipmentWidget::ScreenToSlot(const FVector2D& InScreenPosition, EEquipSlotType& OutSlot) const
{
	if (SlotCanvas == nullptr)
	{
		return false;
	}

	const FGeometry& CanvasGeometry = SlotCanvas->GetCachedGeometry();
	const FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(InScreenPosition);

	for (const EEquipSlotType CandidateSlot : VisibleSlots)
	{
		const FVector2D SlotPosition = GetSlotPosition(CandidateSlot);

		if (LocalPosition.X >= SlotPosition.X && LocalPosition.X < SlotPosition.X + SlotSize
			&& LocalPosition.Y >= SlotPosition.Y && LocalPosition.Y < SlotPosition.Y + SlotSize)
		{
			OutSlot = CandidateSlot;
			return true;
		}
	}

	return false;
}

void UEquipmentWidget::RefreshSlots()
{
	if (SlotCanvas == nullptr)
	{
		return;
	}

	SlotCanvas->ClearChildren();

	if (BoundEquipment == nullptr)
	{
		return;
	}

	BuildSlots();
}

void UEquipmentWidget::BuildSlots()
{
	const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();

	for (const EEquipSlotType SlotType : VisibleSlots)
	{
		UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SlotBorder->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.9f));
		SlotBorder->SetPadding(FMargin(2.0f));
		SlotBorder->SetVisibility(ESlateVisibility::HitTestInvisible);

		FItemInstance EquippedItem;
		const bool bHasItem = BoundEquipment->GetItemInSlot(SlotType, EquippedItem);

		if (bHasItem)
		{
			FItemViewData ViewData;
			ResolveItemViewData(EquippedItem.ItemRow, ViewData);

			UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);

			SlotBorder->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.16f, 1.0f));

			if (IconTexture != nullptr)
			{
				UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				IconImage->SetBrushFromTexture(IconTexture, false);
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
				SlotBorder->SetContent(IconImage);
			}
			else
			{
				UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				Label->SetText(FText::FromString(ViewData.DisplayName));
				Label->SetJustification(ETextJustify::Center);
				Label->SetAutoWrapText(true);
				Label->SetVisibility(ESlateVisibility::HitTestInvisible);
				SlotBorder->SetContent(Label);
			}
		}
		else
		{
			UTextBlock* SlotLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			const FString SlotName = SlotEnum ? SlotEnum->GetDisplayNameTextByValue(static_cast<int64>(SlotType)).ToString() : FString();
			SlotLabel->SetText(FText::FromString(SlotName));
			SlotLabel->SetJustification(ETextJustify::Center);
			SlotLabel->SetAutoWrapText(true);
			SlotLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			SlotBorder->SetContent(SlotLabel);
		}

		UCanvasPanelSlot* CanvasSlot = SlotCanvas->AddChildToCanvas(SlotBorder);
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetPosition(GetSlotPosition(SlotType));
		CanvasSlot->SetSize(FVector2D(SlotSize, SlotSize));
	}
}

FReply UEquipmentWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	EEquipSlotType PressedCandidate;
	if (!ScreenToSlot(InMouseEvent.GetScreenSpacePosition(), PressedCandidate))
	{
		return FReply::Unhandled();
	}

	if (BoundEquipment == nullptr || !BoundEquipment->IsSlotOccupied(PressedCandidate))
	{
		return FReply::Unhandled();
	}

	PressedSlot = PressedCandidate;

	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

void UEquipmentWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (BoundEquipment == nullptr)
	{
		return;
	}

	FItemInstance EquippedItem;
	if (!BoundEquipment->GetItemInSlot(PressedSlot, EquippedItem))
	{
		return;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(EquippedItem.ItemRow, ViewData))
	{
		return;
	}

	UImage* DragVisual = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath))
	{
		DragVisual->SetBrushFromTexture(IconTexture, false);
	}
	DragVisual->SetDesiredSizeOverride(FVector2D(SlotSize, SlotSize));
	DragVisual->SetOpacity(0.7f);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = EquippedItem.InstanceId;
	Operation->GrabOffset = FIntPoint::ZeroValue;
	Operation->SourceInventory = nullptr;
	Operation->SourceEquipment = BoundEquipment;
	Operation->SourceSlot = PressedSlot;
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::MouseDown;

	OutOperation = Operation;
}

bool UEquipmentWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr || BoundEquipment == nullptr)
	{
		return false;
	}

	EEquipSlotType TargetSlot;
	if (!ScreenToSlot(InDragDropEvent.GetScreenSpacePosition(), TargetSlot))
	{
		return false;
	}

	if (Operation->SourceEquipment != nullptr)
	{
		return true;
	}

	if (Operation->SourceInventory == nullptr)
	{
		return false;
	}

	return BoundEquipment->EquipFromInventory(Operation->SourceInventory, Operation->InstanceId, TargetSlot);
}