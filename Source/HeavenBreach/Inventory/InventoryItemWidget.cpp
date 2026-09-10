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
		Image_ItemBackground->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.95f));
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
			// 등급별 배경 그라데이션 색상
			FLinearColor TopColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
			FLinearColor BottomColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
			switch (Grade)
			{
			case EItemGrade::Normal:
				TopColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
				BottomColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
				break;
			case EItemGrade::Rare:
				TopColor = FLinearColor(0.4f, 0.62f, 1.0f, 1.0f);
				BottomColor = FLinearColor(0.08f, 0.14f, 0.35f, 1.0f);
				break;
			case EItemGrade::Epic:
				TopColor = FLinearColor(0.65f, 0.25f, 0.9f, 1.0f);
				BottomColor = FLinearColor(0.18f, 0.05f, 0.3f, 1.0f);
				break;
			case EItemGrade::Legend:
				TopColor = FLinearColor(1.0f, 0.78f, 0.2f, 1.0f);
				BottomColor = FLinearColor(0.42f, 0.26f, 0.04f, 1.0f);
				break;
			case EItemGrade::Relic:
				TopColor = FLinearColor(1.0f, 0.5f, 0.08f, 1.0f);
				BottomColor = FLinearColor(0.42f, 0.16f, 0.02f, 1.0f);
				break;
			case EItemGrade::Ancient:
				TopColor = FLinearColor(1.0f, 0.98f, 0.9f, 1.0f);
				BottomColor = FLinearColor(0.7f, 0.62f, 0.36f, 1.0f);
				break;
			default:
				break;
			}
			BackgroundMID->SetVectorParameterValue(TEXT("TopColor"), TopColor);
			BackgroundMID->SetVectorParameterValue(TEXT("BottomColor"), BottomColor);

			// 테두리는 표시하지 않고 배경색만으로 등급을 표현
			if (Border_GradeFrame != nullptr)
			{
				Border_GradeFrame->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			}
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

	if (SizeBox_Icon != nullptr)
	{
		// ItemSize는 InventoryWidget::BuildItems에서 GetOccupiedSize로 이미
		// 회전 후 기준(뒤바뀐 가로/세로)으로 계산돼서 들어온다. 여기서 아이콘을
		// 그 칸 크기에 맞춰 스케일한 다음 돌리면 눌린 상태에서 회전하게 돼서
		// 찌그러져 보인다. 그래서 반대로 다시 스왑해서 "회전 전 원래 비율"의
		// 박스 크기를 구하고, 그 박스 전체를 통째로 90도 돌린다.
		// (Scale Box가 이 안에서 먼저 원래 비율대로 깔끔하게 맞추고, 그 다음
		// 박스째로 회전하니까 비율이 안 깨진다.)
		const FVector2D NaturalIconSize = Item.bRotated
			? FVector2D(ItemSize.Y * CellSize, ItemSize.X * CellSize)
			: FVector2D(ItemSize.X * CellSize, ItemSize.Y * CellSize);

		// 1x2, 1x3처럼 정사각형이 아닌 길쭉한 아이템은 아이콘 아트 자체 여백 때문에
		// 칸을 다 못 채우고 짧아 보이는 경우가 많다. 그런 아이템에만 살짝 확대해서
		// 보정한다 (정사각형 아이템은 이미 꽉 차 보이므로 그대로 둔다).
		const bool bIsElongated = !FMath::IsNearlyEqual(NaturalIconSize.X, NaturalIconSize.Y);
		const FVector2D ScaledIconSize = bIsElongated
			? NaturalIconSize * ElongatedIconOverscale
			: NaturalIconSize;

		SizeBox_Icon->SetWidthOverride(ScaledIconSize.X);
		SizeBox_Icon->SetHeightOverride(ScaledIconSize.Y);
		SizeBox_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SizeBox_Icon->SetRenderTransformAngle(Item.bRotated ? 90.0f : 0.0f);
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