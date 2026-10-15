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
#include "Components/ProgressBar.h"

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

void UInventoryItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ProgressBar_Cooldown == nullptr)
	{
		return;
	}

	if (!bIsConsumableItem || SourceInventory == nullptr)
	{
		ProgressBar_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const float Percent = SourceInventory->GetItemUseCooldownPercent(Item.ItemRow.RowName);
	if (Percent <= 0.f)
	{
		ProgressBar_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		ProgressBar_Cooldown->SetPercent(Percent);
		ProgressBar_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	// "회전 전 원래" 칸 크기를 한 번만 역산해서 저장(SetPreviewRotated에서 재사용).
	NaturalGridSize = Item.bRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;

	// 상자/몬스터 시체에서 아직 서칭(공개) 안 된 아이템은 실제 정보를 전혀 보여주지 않는다 -
	// 아이콘/이름/등급/내구도/스택 수 전부 숨기고 "?"만 표시한다. bIsRevealed가 false인 동안은
	// bHasViewData가 true여도 IconTexture/Grade를 실제 값으로 채우지 않는 게 핵심.
	const bool bIsRevealed = Item.bRevealed;

	FItemViewData ViewData;
	const bool bHasViewData = ResolveItemViewData(Item.ItemRow, ViewData);
	UTexture2D* IconTexture = (bHasViewData && bIsRevealed) ? LoadItemIcon(ViewData.IconPath) : nullptr;
	const EItemGrade Grade = (bHasViewData && bIsRevealed) ? ViewData.Grade : EItemGrade::Normal;

	const bool bIsBroken = bIsRevealed && bHasViewData && ViewData.IsEquippable() && Item.CurrentDurability == 0.f;
	bIsConsumableItem = bIsRevealed && bHasViewData && ViewData.IsConsumable();
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
			const FLinearColor TopColor = GetInventoryIconTopColor(Grade);
			const FLinearColor MidColor = GetInventoryIconMidColor(Grade);
			const FLinearColor BottomColor = GetInventoryIconBottomColor(Grade);
			BackgroundMID->SetVectorParameterValue(TEXT("TopColor"), TopColor);
			BackgroundMID->SetVectorParameterValue(TEXT("MidColor"), MidColor);
			BackgroundMID->SetVectorParameterValue(TEXT("BottomColor"), BottomColor);

			if (Border_GradeFrame != nullptr)
			{
				FLinearColor FrameColor = GetGradeTopColor(Grade);
				FrameColor.A = 0.45f;
				Border_GradeFrame->SetBrushColor(FrameColor);
			}
		}
	}

	if (Image_ItemIcon != nullptr)
	{
		if (IconTexture != nullptr)
		{
			Image_ItemIcon->SetBrushFromTexture(IconTexture, false);
			Image_ItemIcon->SetColorAndOpacity(bIsBroken
				? FLinearColor(1.0f, 0.35f, 0.35f, 1.0f)
				: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
			Image_ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (SizeBox_Icon != nullptr)
	{
		// NaturalGridSize에 CellSize를 곱해 자연 크기를 구하고, 박스 전체를 회전 상태에 맞춰 90도 돌린다.
		const FVector2D NaturalIconSize = FVector2D(NaturalGridSize.X * CellSize, NaturalGridSize.Y * CellSize);

		const bool bIsElongated = !FMath::IsNearlyEqual(NaturalIconSize.X, NaturalIconSize.Y);
		const FVector2D ScaledIconSize = bIsElongated
			? NaturalIconSize * ElongatedIconOverscale
			: NaturalIconSize;

		SizeBox_Icon->SetWidthOverride(ScaledIconSize.X);
		SizeBox_Icon->SetHeightOverride(ScaledIconSize.Y);
		SizeBox_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SizeBox_Icon->SetRenderTransformAngle(Item.bRotated ? 90.0f : 0.0f);
	}

	if (ProgressBar_Durability != nullptr)
	{
		const bool bTrackDurability = bIsRevealed && ViewData.IsEquippable() && Item.CurrentDurability >= 0.f;

		if (bTrackDurability)
		{
			const float MaxDurability = GetMaxDurabilityForGrade(ViewData.Grade);
			const float Percent = MaxDurability > 0.f
				? FMath::Clamp(Item.CurrentDurability / MaxDurability, 0.f, 1.f)
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
		else
		{
			ProgressBar_Durability->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 실제 진행률/표시 여부는 NativeTick에서 매 프레임 갱신한다(쿨타임이 실시간으로 줄어들기 때문).
	// 여기서는 새로 빌드되는 칸이 이전 칸의 잔상(다른 아이템의 쿨타임 바가 떠 있던 상태)을
	// 물려받지 않도록 일단 접어둔다.
	if (ProgressBar_Cooldown != nullptr)
	{
		ProgressBar_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Text_Broken != nullptr)
	{
		Text_Broken->SetVisibility(bIsBroken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_BrokenBG != nullptr)
	{
		Image_BrokenBG->SetVisibility(bIsBroken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_FallbackName != nullptr)
	{
		if (!bIsRevealed)
		{
			// 서칭 중 - 이름도 모르는 상태이므로 "?"만 보여준다.
			Text_FallbackName->SetText(FText::FromString(TEXT("?")));
			Text_FallbackName->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (IconTexture == nullptr)
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
		if (bIsRevealed && Item.StackCount > 1)
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
		Image_FavoriteStar->SetVisibility(bIsRevealed && Item.bIsFavorite
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
	if (Image_Lock != nullptr)
	{
		Image_Lock->SetVisibility(bIsRevealed && Item.bLocked
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
	if (Image_MaterialBadge != nullptr)
	{
		const bool bIsCraftMaterial = bIsRevealed && bHasViewData && ViewData.ItemType == TEXT("Material");
		Image_MaterialBadge->SetVisibility(bIsCraftMaterial
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
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
	// 아직 서칭 안 된(?) 아이템은 드래그를 시작하지 않는다 - 어차피 서버가 이동/병합을 거부하지만,
	// 드래그가 시작됐다가 아이콘이 제자리로 튕겨 돌아오는 것보다 애초에 안 집히는 게 자연스럽다.
	if (!Item.bRevealed)
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

	Operation->SetDefaultVisual(DragVisualContainer, DragItemWidget, VisualSize);

	SetVisibility(ESlateVisibility::Hidden);
	OutOperation = Operation;
}

void UInventoryItemWidget::HandleDragFinished(UDragDropOperation* Operation)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UInventoryItemWidget::SetPreviewRotated(bool bInRotated)
{
	if (Item.bRotated == bInRotated)
	{
		return;
	}
	Item.bRotated = bInRotated;

	if (SizeBox_Icon == nullptr)
	{
		return;
	}

	// InitializeItem()과 동일한 계산(NaturalGridSize는 고정값이라 재사용 가능).
	const FVector2D NaturalIconSize = FVector2D(NaturalGridSize.X * CellSize, NaturalGridSize.Y * CellSize);

	const bool bIsElongated = !FMath::IsNearlyEqual(NaturalIconSize.X, NaturalIconSize.Y);
	const FVector2D ScaledIconSize = bIsElongated
		? NaturalIconSize * ElongatedIconOverscale
		: NaturalIconSize;

	SizeBox_Icon->SetWidthOverride(ScaledIconSize.X);
	SizeBox_Icon->SetHeightOverride(ScaledIconSize.Y);
	SizeBox_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SizeBox_Icon->SetRenderTransformAngle(Item.bRotated ? 90.0f : 0.0f);
}