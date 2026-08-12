#include "InventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "EquipmentComponent.h"
#include "EquipmentWidget.h"

void UInventoryWidget::BindToInventory(UInventoryComponent* InInventory)
{
	if (BoundInventory != nullptr)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = InInventory;

	if (BoundInventory != nullptr)
	{
		BoundInventory->OnInventoryChanged.AddDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	} 
	SetVisibility(ESlateVisibility::Visible);

	RefreshGrid();
}

void UInventoryWidget::NativeDestruct()
{
	if (BoundInventory != nullptr)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}

	Super::NativeDestruct();
}

void UInventoryWidget::HandleInventoryChanged()
{
	RefreshGrid();
}

void UInventoryWidget::RefreshGrid()
{
	if (GridCanvas == nullptr)
	{
		return;
	}

	GridCanvas->ClearChildren();

	if (BoundInventory == nullptr)
	{
		return;
	}

	BuildBackground();
	BuildItems();
}

void UInventoryWidget::BindToEquipment(UEquipmentComponent* InEquipment)
{
	if (EquipmentPanel != nullptr)
	{
		EquipmentPanel->BindToEquipment(InEquipment, BoundInventory);
	}
}

void UInventoryWidget::SetLinkedContainer(UInventoryComponent* InContainer)
{
	LinkedContainer = InContainer;
}

void UInventoryWidget::SetLinkedEquipment(UEquipmentComponent* InEquipment)
{
	LinkedEquipment = InEquipment;
}

void UInventoryWidget::BuildBackground()
{
	const FIntPoint Size = BoundInventory->GetGridSize();

	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cell->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.9f));
			Cell->SetPadding(FMargin(0.0f));
			Cell->SetVisibility(ESlateVisibility::HitTestInvisible);

			UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(Cell);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(FVector2D(X * CellSize, Y * CellSize));
			CanvasSlot->SetSize(FVector2D(CellSize - 2.0f, CellSize - 2.0f));
			CanvasSlot->SetZOrder(0);
		}
	}
}

void UInventoryWidget::BuildItems()
{
	const TArray<FInventoryEntry>& Entries = BoundInventory->GetEntries();
	for (const FInventoryEntry& Entry : Entries)
	{
		FItemViewData ViewData;
		if (!ResolveItemViewData(Entry.Item.ItemRow, ViewData))
		{
			continue;
		}
		const FIntPoint ItemSize = GetOccupiedSize(Entry.Item, ViewData);
		UBorder* ItemBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ItemBorder->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.16f, 1.0f));
		ItemBorder->SetPadding(FMargin(2.0f));
		ItemBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);
		if (IconTexture != nullptr)
		{
			UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			IconImage->SetBrushFromTexture(IconTexture, false);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			ItemBorder->SetContent(IconImage);
		}
		else
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Label->SetText(FText::FromString(ViewData.DisplayName));
			Label->SetJustification(ETextJustify::Center);
			Label->SetAutoWrapText(true);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			ItemBorder->SetContent(Label);
		}
		UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(ItemBorder);
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetPosition(FVector2D(Entry.Position.X * CellSize, Entry.Position.Y * CellSize));
		CanvasSlot->SetSize(FVector2D(ItemSize.X * CellSize - 2.0f, ItemSize.Y * CellSize - 2.0f));
		CanvasSlot->SetZOrder(1);
		if (Entry.Item.StackCount > 1)
		{
			UTextBlock* CountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			CountLabel->SetText(FText::AsNumber(Entry.Item.StackCount));
			CountLabel->SetJustification(ETextJustify::Right);
			CountLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* CountSlot = GridCanvas->AddChildToCanvas(CountLabel);
			CountSlot->SetAutoSize(true);
			CountSlot->SetPosition(FVector2D(
				(Entry.Position.X + ItemSize.X) * CellSize - 20.0f,
				(Entry.Position.Y + ItemSize.Y) * CellSize - 20.0f));
			CountSlot->SetZOrder(2);
		}
	}
}

bool UInventoryWidget::ScreenToCell(const FVector2D& InScreenPosition, FIntPoint& OutCell) const
{
	if (GridCanvas == nullptr || BoundInventory == nullptr || CellSize <= 0.0f)
	{
		return false;
	}

	const FGeometry& CanvasGeometry = GridCanvas->GetCachedGeometry();
	const FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(InScreenPosition);

	const FIntPoint Cell(
		FMath::FloorToInt(LocalPosition.X / CellSize),
		FMath::FloorToInt(LocalPosition.Y / CellSize));

	if (!BoundInventory->IsValidCoord(Cell))
	{
		return false;
	}

	OutCell = Cell;
	return true;
}

bool UInventoryWidget::ScreenToLocal(const FVector2D& InScreenPosition, FVector2D& OutLocal) const
{
	if (GridCanvas == nullptr)
	{
		return false;
	}

	const FGeometry& CanvasGeometry = GridCanvas->GetCachedGeometry();
	OutLocal = CanvasGeometry.AbsoluteToLocal(InScreenPosition);

	return true;
}

bool UInventoryWidget::HandleRightClick(FIntPoint InCell)
{
	if (BoundInventory == nullptr)
	{
		return false;
	}

	const int32 EntryIndex = BoundInventory->GetEntryIndexAt(InCell);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance Item = BoundInventory->GetEntries()[EntryIndex].Item;

	if (LinkedContainer != nullptr)
	{
		return LinkedContainer->TransferItemFromAuto(BoundInventory, Item.InstanceId);
	}

	if (LinkedEquipment == nullptr)
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(Item.ItemRow, ViewData))
	{
		return false;
	}

	if (!ViewData.IsEquippable())
	{
		return false;
	}

	return LinkedEquipment->EquipFromInventory(BoundInventory, Item.InstanceId, ViewData.EquipSlotType);
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetEffectingButton().IsMouseButton())
	{
		return FReply::Unhandled();
	}

	FIntPoint Cell;
	if (!ScreenToCell(InMouseEvent.GetScreenSpacePosition(), Cell))
	{
		return FReply::Unhandled();
	}

	if (BoundInventory->GetEntryIndexAt(Cell) == INDEX_NONE)
	{
		return FReply::Unhandled();
	}

	PressedCell = Cell;

	ScreenToLocal(InMouseEvent.GetScreenSpacePosition(), PressedLocalPosition);
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}

	FIntPoint Cell;
	if (!ScreenToCell(InMouseEvent.GetScreenSpacePosition(), Cell))
	{
		return FReply::Unhandled();
	}

	if (!HandleRightClick(Cell))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled();
}

void UInventoryWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (BoundInventory == nullptr)
	{
		return;
	}
	const int32 EntryIndex = BoundInventory->GetEntryIndexAt(PressedCell);
	if (EntryIndex == INDEX_NONE)
	{
		return;
	}
	const FInventoryEntry& Entry = BoundInventory->GetEntries()[EntryIndex];
	FItemViewData ViewData;
	if (!ResolveItemViewData(Entry.Item.ItemRow, ViewData))
	{
		return;
	}
	const FIntPoint ItemSize = GetOccupiedSize(Entry.Item, ViewData);
	const FVector2D VisualSize(ItemSize.X * CellSize, ItemSize.Y * CellSize);
	UImage* DragVisual = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath))
	{
		DragVisual->SetBrushFromTexture(IconTexture, false);
	}
	DragVisual->SetDesiredSizeOverride(VisualSize);
	DragVisual->SetOpacity(0.7f);
	const FVector2D ItemOrigin(Entry.Position.X * CellSize, Entry.Position.Y * CellSize);
	const FVector2D PixelOffset = PressedLocalPosition - ItemOrigin;
	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = Entry.Item.InstanceId;
	Operation->GrabOffset = PressedCell - Entry.Position;
	Operation->GrabPixelOffset = PixelOffset;
	Operation->SourceInventory = BoundInventory;
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::TopLeft;
	Operation->Offset = FVector2D(
		-PixelOffset.X / VisualSize.X,
		-PixelOffset.Y / VisualSize.Y);
	OutOperation = Operation;
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr || BoundInventory == nullptr || CellSize <= 0.0f)
	{
		return false;
	}

	FVector2D LocalPosition;
	if (!ScreenToLocal(InDragDropEvent.GetScreenSpacePosition(), LocalPosition))
	{
		return false;
	}

	const FVector2D ItemOrigin = LocalPosition - Operation->GrabPixelOffset;

	const FIntPoint TargetPosition(
		FMath::RoundToInt(ItemOrigin.X / CellSize),
		FMath::RoundToInt(ItemOrigin.Y / CellSize));

	if (Operation->SourceEquipment != nullptr)
	{
		return Operation->SourceEquipment->UnequipToInventoryAt(
			Operation->SourceSlot, BoundInventory, TargetPosition);
	}

	if (Operation->SourceInventory == BoundInventory)
	{
		return BoundInventory->MoveItem(Operation->InstanceId, TargetPosition);
	}

	return BoundInventory->TransferItemFrom(
		Operation->SourceInventory, Operation->InstanceId, TargetPosition);
}