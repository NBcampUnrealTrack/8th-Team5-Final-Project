#include "InventoryItemWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
	const EItemGrade Grade = bHasViewData ? ViewData.Grade : EItemGrade::Normal;
	if (Image_ItemBackground != nullptr)
	{
		Image_ItemBackground->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
	}

	if (Image_ItemBackground != nullptr)
	{
		if (BackgroundMID == nullptr)
		{
			if (UMaterialInterface* BaseMat = Cast<UMaterialInterface>(Image_ItemBackground->GetBrush().GetResourceObject()))
			{
				BackgroundMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				Image_ItemBackground->SetBrushFromMaterial(BackgroundMID);
			}
		}
		if (BackgroundMID != nullptr)
		{
			FLinearColor TopColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			FLinearColor BottomColor = FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);
			switch (Grade)
			{
			case EItemGrade::Normal:
				TopColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
				BottomColor = FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);
				break;
			case EItemGrade::Rare:
				TopColor = FLinearColor(0.55f, 0.7f, 1.0f, 1.0f);
				BottomColor = FLinearColor(0.15f, 0.22f, 0.4f, 1.0f);
				break;
			case EItemGrade::Epic:
				TopColor = FLinearColor(0.32f, 0.06f, 0.48f, 1.0f);
				BottomColor = FLinearColor(0.10f, 0.02f, 0.16f, 1.0f);
				break;
			case EItemGrade::Legend:
				TopColor = FLinearColor(1.0f, 0.78f, 0.4f, 1.0f);
				BottomColor = FLinearColor(0.45f, 0.32f, 0.12f, 1.0f);
				break;
			default:
				break;
			}
			BackgroundMID->SetVectorParameterValue(TEXT("TopColor"), TopColor);
			BackgroundMID->SetVectorParameterValue(TEXT("BottomColor"), BottomColor);
		}
	}

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
	if (Image_FavoriteStar != nullptr)
	{
		Image_FavoriteStar->SetVisibility(Item.bIsFavorite
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
	if (Border_ItemFrame != nullptr)
	{
		Border_ItemFrame->SetVisibility(Item.bIsFavorite
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
	if (Item.bIsFavorite && Anim_FavoritePulse != nullptr)
	{
		PlayAnimation(Anim_FavoritePulse, 0.0f, 0, EUMGSequencePlayMode::PingPong);
	}
	else if (Anim_FavoritePulse != nullptr)
	{
		StopAnimation(Anim_FavoritePulse);
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
	// Use the pointer position at the moment the drag is detected. This keeps
	// the decorator and drop calculation aligned after Slate's drag threshold.
	GrabPixelOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D VisualSize(
		ItemSize.X * CellSize - 2.0f,
		ItemSize.Y * CellSize - 2.0f);

	UInventoryItemWidget* DragItemWidget = CreateWidget<UInventoryItemWidget>(
		GetOwningPlayer(), GetClass());
	if (DragItemWidget == nullptr)
	{
		return;
	}
	DragItemWidget->InitializeItem(SourceInventory, Item, ItemSize, CellSize);
	DragItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	DragItemWidget->SetRenderOpacity(0.7f);

	USizeBox* DragVisualContainer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DragVisualContainer->SetWidthOverride(VisualSize.X);
	DragVisualContainer->SetHeightOverride(VisualSize.Y);
	DragVisualContainer->SetContent(DragItemWidget);
	DragVisualContainer->SetVisibility(ESlateVisibility::HitTestInvisible);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = Item.InstanceId;
	Operation->GrabOffset = FIntPoint(
		FMath::Clamp(FMath::FloorToInt(GrabPixelOffset.X / CellSize), 0, ItemSize.X - 1),
		FMath::Clamp(FMath::FloorToInt(GrabPixelOffset.Y / CellSize), 0, ItemSize.Y - 1));
	Operation->GrabPixelOffset = GrabPixelOffset;
	Operation->SourceInventory = SourceInventory;
	Operation->DefaultDragVisual = DragVisualContainer;
	Operation->Pivot = EDragPivot::MouseDown;
	Operation->Offset = FVector2D::ZeroVector;
	Operation->bSplitRequested = bSplitRequested;
	Operation->OnDrop.AddDynamic(this, &UInventoryItemWidget::HandleDragFinished);
	Operation->OnDragCancelled.AddDynamic(this, &UInventoryItemWidget::HandleDragFinished);

	SetVisibility(ESlateVisibility::Hidden);
	OutOperation = Operation;
}

void UInventoryItemWidget::HandleDragFinished(UDragDropOperation* Operation)
{
	SetVisibility(ESlateVisibility::Visible);
}