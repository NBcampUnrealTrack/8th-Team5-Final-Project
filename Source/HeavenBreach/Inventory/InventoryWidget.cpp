#include "InventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"

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
		UTexture2D* IconTexture = nullptr;
		if (!ViewData.IconPath.IsEmpty())
		{
			IconTexture = LoadObject<UTexture2D>(nullptr, *ViewData.IconPath);
		}
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

	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
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

	UImage* DragVisual = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!ViewData.IconPath.IsEmpty())
	{
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *ViewData.IconPath))
		{
			DragVisual->SetBrushFromTexture(IconTexture, false);
		}
	}
	DragVisual->SetDesiredSizeOverride(FVector2D(ItemSize.X * CellSize, ItemSize.Y * CellSize));
	DragVisual->SetOpacity(0.7f);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = Entry.Item.InstanceId;
	Operation->GrabOffset = PressedCell - Entry.Position;
	Operation->SourceInventory = BoundInventory;
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::MouseDown;

	OutOperation = Operation;
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr || BoundInventory == nullptr)
	{
		return false;
	}

	FIntPoint DropCell;
	if (!ScreenToCell(InDragDropEvent.GetScreenSpacePosition(), DropCell))
	{
		return false;
	}

	const FIntPoint TargetPosition = DropCell - Operation->GrabOffset;

	return BoundInventory->MoveItem(Operation->InstanceId, TargetPosition);
}