#include "InventoryItemWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"

void UInventoryItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// A transparent, hit-testable root lets this widget receive input while the
	// existing item border and icon continue to render underneath it.
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		UBorder* InputSurface = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		InputSurface->SetBrushColor(FLinearColor::Transparent);
		InputSurface->SetPadding(FMargin(0.0f));
		InputSurface->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = InputSurface;
	}
}

void UInventoryItemWidget::InitializeItem(
	UInventoryComponent* InSourceInventory,
	const FItemInstance& InItem,
	FIntPoint InItemSize,
	float InCellSize)
{
	SourceInventory = InSourceInventory;
	Item = InItem;
	ItemSize = InItemSize;
	CellSize = InCellSize;

	FItemViewData ViewData;
	const bool bHasViewData = ResolveItemViewData(Item.ItemRow, ViewData);
	UTexture2D* IconTexture = bHasViewData ? LoadItemIcon(ViewData.IconPath) : nullptr;

	if (Image_ItemIcon != nullptr)
	{
		if (IconTexture != nullptr)
		{
			Image_ItemIcon->SetBrushFromTexture(IconTexture, false);
			Image_ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Text_FallbackName != nullptr)
	{
		if (IconTexture == nullptr)
		{
			Text_FallbackName->SetText(bHasViewData
				? FText::FromString(ViewData.DisplayName)
				: FText::FromName(Item.ItemRow.RowName));
			Text_FallbackName->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_FallbackName->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Text_StackCount != nullptr)
	{
		if (Item.StackCount > 1)
		{
			Text_StackCount->SetText(FText::AsNumber(Item.StackCount));
			Text_StackCount->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_StackCount->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	SetVisibility(ESlateVisibility::Visible);
}

FReply UInventoryItemWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	GrabPixelOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	bSplitRequested = InMouseEvent.IsControlDown() && Item.StackCount > 1;
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

void UInventoryItemWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (SourceInventory == nullptr || CellSize <= 0.0f)
	{
		return;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(Item.ItemRow, ViewData))
	{
		return;
	}

	// Use the pointer position at the moment the drag is detected. This keeps
	// the decorator and drop calculation aligned after Slate's drag threshold.
	GrabPixelOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D VisualSize(ItemSize.X * CellSize, ItemSize.Y * CellSize);

	UImage* DragVisual = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath))
	{
		DragVisual->SetBrushFromTexture(IconTexture, false);
	}
	DragVisual->SetDesiredSizeOverride(VisualSize);
	DragVisual->SetOpacity(0.7f);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = Item.InstanceId;
	Operation->GrabOffset = FIntPoint(
		FMath::Clamp(FMath::FloorToInt(GrabPixelOffset.X / CellSize), 0, ItemSize.X - 1),
		FMath::Clamp(FMath::FloorToInt(GrabPixelOffset.Y / CellSize), 0, ItemSize.Y - 1));
	Operation->GrabPixelOffset = GrabPixelOffset;
	Operation->SourceInventory = SourceInventory;
	Operation->DefaultDragVisual = DragVisual;
	Operation->Pivot = EDragPivot::MouseDown;
	Operation->Offset = FVector2D::ZeroVector;
	Operation->bSplitRequested = bSplitRequested;
	OutOperation = Operation;
}
