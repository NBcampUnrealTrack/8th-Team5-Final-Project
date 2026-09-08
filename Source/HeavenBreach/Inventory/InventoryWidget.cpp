#include "InventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "InventoryCellWidget.h"
#include "InventoryItemWidget.h"
#include "EquipmentComponent.h"
#include "EquipmentWidget.h"
#include "Item/ItemActionMenuWidget.h"
#include "Item/Item.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Item/UItemSplitPopupWidget.h"
#include "Item/ItemInfoPopupWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Player/HBPlayerController.h"
#include "ShopComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/GameViewportClient.h"
#include "Player/PlayerState/HB_PlayerState.h"

UInventoryWidget::UInventoryWidget()
{
	// 활성 상태에서 게임 입력을 차단하고 커서를 띄운다.
	// 드래그&드롭 때문에 마우스는 캡처하지 않는다.
	InputMode = EHBWidgetInputMode::Menu;

	// ESC 로 자기 자신을 닫는다. 스택이 아래 위젯을 자동 재활성한다.
	bIsBackHandler = true;
}

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

void UInventoryWidget::SetParentContainerWidget(UUserWidget* InParentWidget)
{
	ParentContainerWidget = InParentWidget;
}

void UInventoryWidget::SetEquipmentPanelVisible(bool bVisible)
{
	if (EquipmentPanel != nullptr)
	{
		EquipmentPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UInventoryWidget::SetShopComponent(UShopComponent* InShop)
{
	if (LinkedShop != nullptr)
	{
		LinkedShop->OnBasketChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}

	LinkedShop = InShop;

	if (LinkedShop != nullptr)
	{
		LinkedShop->OnBasketChanged.AddDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}
}

void UInventoryWidget::ScrollRows(int32 InDelta)
{
	const int32 NewOffset = FMath::Clamp(ScrollOffsetRow + InDelta, 0, GetMaxScrollRow());

	if (NewOffset == ScrollOffsetRow)
	{
		return;
	}

	ScrollOffsetRow = NewOffset;

	RefreshGrid();

	OnScrollChanged.Broadcast();
}

float UInventoryWidget::GetScrollRatio() const
{
	const int32 MaxRow = GetMaxScrollRow();

	if (MaxRow <= 0)
	{
		return 0.0f;
	}

	return 1.0f - static_cast<float>(ScrollOffsetRow) / static_cast<float>(MaxRow);
}

void UInventoryWidget::SetScrollByRatio(float InRatio)
{
	const int32 MaxRow = GetMaxScrollRow();

	if (MaxRow <= 0)
	{
		return;
	}

	const int32 NewOffset = FMath::Clamp(FMath::RoundToInt((1.0f - InRatio) * MaxRow), 0, MaxRow);

	if (NewOffset == ScrollOffsetRow)
	{
		return;
	}

	ScrollOffsetRow = NewOffset;

	RefreshGrid();
}

bool UInventoryWidget::IsScrollable() const
{
	return GetMaxScrollRow() > 0;
}

void UInventoryWidget::PrepareForHide()
{
	ClearHoveredCell();
	ClearPlacementPreview();

	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
	{
		// CancelDragDrop()이 Operation의 OnDragCancelled를 호출해서 드래그 아이콘도 같이 정리된다.
		FSlateApplication::Get().CancelDragDrop();
	}
	if (ActiveContextMenu)
	{
		ActiveContextMenu->RemoveFromParent();
		ActiveContextMenu = nullptr;
	}
	if (ActiveSplitPopup)
	{
		ActiveSplitPopup->RemoveFromParent();
		ActiveSplitPopup = nullptr;
	}
	if (ActiveInfoPopup)
	{
		ActiveInfoPopup->RemoveFromParent();
		ActiveInfoPopup = nullptr;
	}

	bHasValidPress = false;
	bPressedWithCtrl = false;
}

void UInventoryWidget::NativeDestruct()
{
	if (BoundInventory != nullptr)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}
	if (LinkedShop != nullptr)
	{
		LinkedShop->OnBasketChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	}
	// 드래그 중에 인벤토리 창이 닫히면(Tab 키 등) CancelDragDrop()이 Operation의
	// OnDragCancelled를 호출해서 뷰포트에 올려둔 드래그 아이콘도 같이 정리된다.
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
	{
		FSlateApplication::Get().CancelDragDrop();
	}
	Super::NativeDestruct();
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

FReply UInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (LinkedContainer == nullptr)
		{
			if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
			{
				PC->RequestCloseInventory();
			}
			return FReply::Handled();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Three || InKeyEvent.GetKey() == EKeys::Four)
	{
		const int32 SlotIndex = (InKeyEvent.GetKey() == EKeys::Three) ? 2 : 3;
		if (HandleQuickSlotRegister(SlotIndex))
		{
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInventoryWidget::HandleInventoryChanged()
{
	RefreshGrid();
}

void UInventoryWidget::RefreshGrid()
{
	HoveredCell = FIntPoint(-1, -1);
	PlacementPreviewWidgets.Reset();
	CellWidgets.Reset();
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

void UInventoryWidget::SetOwningInventory(UInventoryComponent* InOwningInventory)
{
	OwningInventory = InOwningInventory;
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
	const int32 StartRow = ScrollOffsetRow;
	const int32 EndRow = FMath::Min(Size.Y, ScrollOffsetRow + GetVisibleRowCount());

	CellWidgets.SetNum(Size.X * Size.Y);

	for (int32 Y = StartRow; Y < EndRow; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			if (InventoryCellWidgetClass != nullptr)
			{
				UInventoryCellWidget* CellWidget = CreateWidget<UInventoryCellWidget>(
					GetOwningPlayer(), InventoryCellWidgetClass);
				if (CellWidget != nullptr)
				{
					CellWidget->InitializeCell(FIntPoint(X, Y));
					CellWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
					UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(CellWidget);
					CanvasSlot->SetAutoSize(false);
					CanvasSlot->SetPosition(FVector2D(X * CellSize, (Y - ScrollOffsetRow) * CellSize));
					CanvasSlot->SetSize(FVector2D(CellSize - 2.0f, CellSize - 2.0f));
					CanvasSlot->SetZOrder(0);
					CellWidgets[Y * Size.X + X] = CellWidget;
					continue;
				}
			}

			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cell->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.9f));
			Cell->SetPadding(FMargin(0.0f));
			Cell->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(Cell);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(FVector2D(X * CellSize, (Y - ScrollOffsetRow) * CellSize));
			CanvasSlot->SetSize(FVector2D(CellSize - 2.0f, CellSize - 2.0f));
			CanvasSlot->SetZOrder(0);
		}
	}
}

void UInventoryWidget::BuildItems()
{
	const int32 StartRow = ScrollOffsetRow;
	const int32 EndRow = ScrollOffsetRow + GetVisibleRowCount();

	const TArray<FInventoryEntry>& Entries = BoundInventory->GetEntries();
	for (const FInventoryEntry& Entry : Entries)
	{
		FItemViewData ViewData;
		if (!ResolveItemViewData(Entry.Item.ItemRow, ViewData))
		{
			continue;
		}
		const FIntPoint ItemSize = GetOccupiedSize(Entry.Item, ViewData);
		if (Entry.Position.Y + ItemSize.Y <= StartRow || Entry.Position.Y >= EndRow)
		{
			continue;
		}
		const float DrawY = (Entry.Position.Y - ScrollOffsetRow) * CellSize;
		UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);
		if (InventoryItemWidgetClass != nullptr)
		{
			UInventoryItemWidget* ItemWidget = CreateWidget<UInventoryItemWidget>(
				GetOwningPlayer(), InventoryItemWidgetClass);
			if (ItemWidget != nullptr)
			{
				ItemWidget->InitializeItem(BoundInventory, Entry.Item, ItemSize, CellSize);
				ItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
				UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(ItemWidget);
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetPosition(FVector2D(Entry.Position.X * CellSize, DrawY));
				CanvasSlot->SetSize(FVector2D(ItemSize.X * CellSize - 2.0f, ItemSize.Y * CellSize - 2.0f));
				CanvasSlot->SetZOrder(1);
				if (LinkedShop != nullptr && LinkedShop->IsInBasket(Entry.Item.InstanceId))
				{
					ItemWidget->SetRenderOpacity(0.4f);
				}
				continue;
			}
		}
		UBorder* ItemBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ItemBorder->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.16f, 1.0f));
		ItemBorder->SetPadding(FMargin(2.0f));
		ItemBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (LinkedShop != nullptr && LinkedShop->IsInBasket(Entry.Item.InstanceId))
		{
			ItemBorder->SetRenderOpacity(0.4f);
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
		CanvasSlot->SetPosition(FVector2D(Entry.Position.X * CellSize, DrawY));
		CanvasSlot->SetSize(FVector2D(ItemSize.X * CellSize - 2.0f, ItemSize.Y * CellSize - 2.0f));
		CanvasSlot->SetZOrder(1);
		if (Entry.Item.StackCount > 1)
		{
			UTextBlock::StaticClass();
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
		FMath::FloorToInt(LocalPosition.Y / CellSize) + ScrollOffsetRow);
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

void UInventoryWidget::SetHoveredCell(FIntPoint InCell)
{
	if (HoveredCell == InCell || BoundInventory == nullptr)
	{
		return;
	}

	ClearHoveredCell();

	const FIntPoint GridSize = BoundInventory->GetGridSize();
	const int32 CellIndex = InCell.Y * GridSize.X + InCell.X;
	if (CellWidgets.IsValidIndex(CellIndex) && CellWidgets[CellIndex] != nullptr)
	{
		CellWidgets[CellIndex]->SetVisualState(EInventoryCellVisualState::Hovered);
		HoveredCell = InCell;
	}
}

void UInventoryWidget::ClearHoveredCell()
{
	if (BoundInventory != nullptr)
	{
		const FIntPoint GridSize = BoundInventory->GetGridSize();
		const int32 CellIndex = HoveredCell.Y * GridSize.X + HoveredCell.X;
		if (CellWidgets.IsValidIndex(CellIndex) && CellWidgets[CellIndex] != nullptr)
		{
			CellWidgets[CellIndex]->SetVisualState(EInventoryCellVisualState::Default);
		}
	}

	HoveredCell = FIntPoint(-1, -1);
}

FReply UInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FIntPoint Cell;
	if (ScreenToCell(InMouseEvent.GetScreenSpacePosition(), Cell))
	{
		SetHoveredCell(Cell);
	}
	else
	{
		ClearHoveredCell();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UInventoryWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (GetMaxScrollRow() <= 0)
	{
		return FReply::Unhandled();
	}

	const float WheelDelta = InMouseEvent.GetWheelDelta();

	ScrollRows(WheelDelta > 0.0f ? -1 : 1);

	return FReply::Handled();
}

void UInventoryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	ClearHoveredCell();
	Super::NativeOnMouseLeave(InMouseEvent);
}

bool UInventoryWidget::UpdatePlacementPreview(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);
	if (Operation == nullptr || BoundInventory == nullptr || CellSize <= 0.0f)
	{
		ClearPlacementPreview();
		return false;
	}

	FItemInstance DraggedItem;
	int32 IgnoredEntryIndex = INDEX_NONE;
	if (Operation->SourceInventory != nullptr)
	{
		const int32 SourceIndex = Operation->SourceInventory->FindEntryIndexById(Operation->InstanceId);
		if (!Operation->SourceInventory->GetEntries().IsValidIndex(SourceIndex))
		{
			ClearPlacementPreview();
			return false;
		}

		DraggedItem = Operation->SourceInventory->GetEntries()[SourceIndex].Item;
		if (Operation->SourceInventory == BoundInventory)
		{
			IgnoredEntryIndex = SourceIndex;
		}
	}
	else if (Operation->SourceEquipment != nullptr)
	{
		if (!Operation->SourceEquipment->GetItemInSlot(Operation->SourceSlot, DraggedItem))
		{
			ClearPlacementPreview();
			return false;
		}
	}
	else
	{
		ClearPlacementPreview();
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(DraggedItem.ItemRow, ViewData))
	{
		ClearPlacementPreview();
		return false;
	}

	FVector2D LocalPosition;
	if (!ScreenToLocal(InDragDropEvent.GetScreenSpacePosition(), LocalPosition))
	{
		ClearPlacementPreview();
		return false;
	}

	const FIntPoint ItemSize = GetOccupiedSize(DraggedItem, ViewData);
	FIntPoint TargetPosition;
	if (Operation->SourceInventory == BoundInventory)
	{
		const FVector2D ItemOrigin = LocalPosition - Operation->GrabPixelOffset;
		TargetPosition = FIntPoint(
			FMath::RoundToInt(ItemOrigin.X / CellSize),
			FMath::RoundToInt(ItemOrigin.Y / CellSize) + ScrollOffsetRow);
	}
	else
	{
		const FIntPoint DropCell(
			FMath::FloorToInt(LocalPosition.X / CellSize),
			FMath::FloorToInt(LocalPosition.Y / CellSize) + ScrollOffsetRow);
		TargetPosition = DropCell - Operation->GrabOffset;
	}

	const bool bCanPlace = BoundInventory->CanPlaceAt(ItemSize, TargetPosition, IgnoredEntryIndex);

	const EInventoryCellVisualState PreviewState = bCanPlace
		? EInventoryCellVisualState::PlacementValid
		: EInventoryCellVisualState::PlacementInvalid;

	ClearHoveredCell();
	ClearPlacementPreview();

	for (int32 OffsetY = 0; OffsetY < ItemSize.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < ItemSize.X; ++OffsetX)
		{
			const FIntPoint Cell = TargetPosition + FIntPoint(OffsetX, OffsetY);
			if (!BoundInventory->IsValidCoord(Cell))
			{
				continue;
			}

			if (InventoryCellWidgetClass != nullptr)
			{
				UInventoryCellWidget* PreviewWidget = CreateWidget<UInventoryCellWidget>(
					GetOwningPlayer(), InventoryCellWidgetClass);
				if (PreviewWidget != nullptr)
				{
					PreviewWidget->InitializeCell(Cell);
					PreviewWidget->SetVisualState(PreviewState);
					PreviewWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

					UCanvasPanelSlot* PreviewSlot = GridCanvas->AddChildToCanvas(PreviewWidget);
					PreviewSlot->SetAutoSize(false);
					PreviewSlot->SetPosition(FVector2D(Cell.X * CellSize, (Cell.Y - ScrollOffsetRow) * CellSize));
					PreviewSlot->SetSize(FVector2D(CellSize - 2.0f, CellSize - 2.0f));
					PreviewSlot->SetZOrder(3);
					PlacementPreviewWidgets.Add(PreviewWidget);
				}
			}
		}
	}

	return true;
}

void UInventoryWidget::ClearPlacementPreview()
{
	for (UInventoryCellWidget* PreviewWidget : PlacementPreviewWidgets)
	{
		if (PreviewWidget != nullptr)
		{
			PreviewWidget->RemoveFromParent();
		}
	}

	PlacementPreviewWidgets.Reset();
}

int32 UInventoryWidget::GetVisibleRowCount() const
{
	if (BoundInventory == nullptr)
	{
		return 0;
	}

	const int32 TotalRows = BoundInventory->GetGridSize().Y;

	if (VisibleRows <= 0 || VisibleRows >= TotalRows)
	{
		return TotalRows;
	}

	return VisibleRows;
}

int32 UInventoryWidget::GetMaxScrollRow() const
{
	if (BoundInventory == nullptr)
	{
		return 0;
	}

	return FMath::Max(0, BoundInventory->GetGridSize().Y - GetVisibleRowCount());
}

bool UInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 드래그 아이콘 위치 갱신은 Operation의 OnDragged 이벤트가 매 프레임 알아서 처리한다
	// (패널 안/밖 상관없이 계속 따라다닌다).
	return UpdatePlacementPreview(InDragDropEvent, InOperation)
		|| Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

void UInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearPlacementPreview();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}
void UInventoryWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearPlacementPreview();
	// 드래그 아이콘 정리는 Operation의 OnDragCancelled 이벤트가 알아서 정확히 한 번 처리한다.
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

bool UInventoryWidget::HandleRightClick(FIntPoint InCell)
{
	if (BoundInventory == nullptr)
	{
		return false;
	}
	UInventoryComponent* RequestSender = OwningInventory != nullptr ? OwningInventory : BoundInventory;
	const int32 EntryIndex = BoundInventory->GetEntryIndexAt(InCell);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}
	const FItemInstance Item = BoundInventory->GetEntries()[EntryIndex].Item;

	if (OnItemActivated.IsBound())
	{
		OnItemActivated.Broadcast(Item.InstanceId);
		return true;
	}

	if (LinkedShop != nullptr)
	{
		return LinkedShop->AddToSellBasket(Item.InstanceId);
	}
	if (LinkedContainer != nullptr)
	{
		if (BoundInventory == RequestSender)
		{
			RequestSender->Server_TransferItemAuto(LinkedContainer, Item.InstanceId);
		}
		else
		{
			RequestSender->Server_TransferItemFromAuto(BoundInventory, Item.InstanceId);
		}
		return true;
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
	RequestSender->Server_EquipItem(LinkedEquipment, Item.InstanceId, ViewData.EquipSlotType);
	return true;
}

bool UInventoryWidget::HandleContextMenu(FIntPoint InCell)
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
	if (ItemContextMenuClass == nullptr)
	{
		return false;
	}

	const FItemInstance Item = BoundInventory->GetEntries()[EntryIndex].Item;
	FItemViewData ViewData;
	ResolveItemViewData(Item.ItemRow, ViewData);

	if (ActiveContextMenu != nullptr)
	{
		ActiveContextMenu->RemoveFromParent();
		ActiveContextMenu = nullptr;
	}

	ActiveContextMenu = CreateWidget<UItemActionMenuWidget>(GetOwningPlayer(), ItemContextMenuClass);

	if (ActiveContextMenu == nullptr)
	{
		return false;
	}
	const bool bCanUse = ViewData.IsConsumable();
	const bool bCanSplit = Item.StackCount > 1;
	ActiveContextMenu->SetupForItem(bCanUse, bCanSplit, Item.bIsFavorite);
	const FGuid InstanceId = Item.InstanceId;
	ActiveContextMenu->OnUseRequested.AddLambda([this, InstanceId]()
		{
			if (BoundInventory != nullptr)
			{
				BoundInventory->Server_UseItem(InstanceId);
			}
		});
	ActiveContextMenu->OnSplitRequested.AddLambda([this, InstanceId, StackCount = Item.StackCount]()
		{
			if (ItemSplitPopupClass == nullptr || StackCount <= 1)
			{
				return;
			}
			if (ActiveSplitPopup != nullptr)
			{
				ActiveSplitPopup->RemoveFromParent();
				ActiveSplitPopup = nullptr;
			}
			ActiveSplitPopup = CreateWidget<UItemSplitPopupWidget>(GetOwningPlayer(), ItemSplitPopupClass);
			if (ActiveSplitPopup == nullptr)
			{
				return;
			}
			ActiveSplitPopup->SetupRange(1, StackCount - 1);
			ActiveSplitPopup->OnSplitConfirmed.AddLambda([this, InstanceId](int32 Quantity)
				{
					if (BoundInventory != nullptr)
					{
						BoundInventory->Server_SplitStack(InstanceId, Quantity, FIntPoint(-1, -1));
					}

				});
			ActiveSplitPopup->OnPopupClosed.AddLambda([this]()
				{
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				});
			ActiveSplitPopup->AddToViewport(100);
			ActiveSplitPopup->SetKeyboardFocus();
			const FVector2D SplitViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
			ActiveSplitPopup->SetPositionInViewport(SplitViewportPosition, false);
		});
	ActiveContextMenu->OnDiscardRequested.AddLambda([this, InstanceId]()
		{
			if (BoundInventory != nullptr)
			{
				BoundInventory->Server_DropItem(InstanceId);
			}
		});
	ActiveContextMenu->OnFavoriteRequested.AddLambda([this, InstanceId]()
		{
			if (BoundInventory != nullptr)
			{
				BoundInventory->Server_ToggleFavorite(InstanceId);
			}
		});
	ActiveContextMenu->OnInfoRequested.AddLambda([this, Item, ViewData]()
		{
			if (ItemInfoPopupClass == nullptr)
			{
				return;
			}
			if (ActiveInfoPopup != nullptr)
			{
				ActiveInfoPopup->RemoveFromParent();
				ActiveInfoPopup = nullptr;
			}
			ActiveInfoPopup = CreateWidget<UItemInfoPopupWidget>(GetOwningPlayer(), ItemInfoPopupClass);
			if (ActiveInfoPopup == nullptr)
			{
				return;
			}
			UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);
			FString StatText;
			if (const UDataTable* ItemDataTable = Item.ItemRow.DataTable)
			{
				const UScriptStruct* RowStruct = ItemDataTable->GetRowStruct();
				if (RowStruct == FWeaponData::StaticStruct())
				{
					if (const FWeaponData* WeaponRow = ItemDataTable->FindRow<FWeaponData>(Item.ItemRow.RowName, TEXT("ItemInfoPopup")))
					{
						const FText WeaponStatFormat = FText::FromString(TEXT("공격력: {0}\n사거리: {1}\n공격 속도: {2}초"));
						StatText = FText::Format(WeaponStatFormat,
							FText::AsNumber(WeaponRow->AttackPower),
							FText::AsNumber(WeaponRow->AttackRange),
							FText::AsNumber(WeaponRow->AttackInterval)).ToString();
					}
				}
				else if (RowStruct == FShieldData::StaticStruct())
				{
					if (const FShieldData* ShieldRow = ItemDataTable->FindRow<FShieldData>(Item.ItemRow.RowName, TEXT("ItemInfoPopup")))
					{
						const FText ShieldStatFormat = FText::FromString(TEXT("피해 감소: {0}%\n방어 범위: {1}도"));
						StatText = FText::Format(ShieldStatFormat,
							FText::AsNumber(ShieldRow->DamageReduction),
							FText::AsNumber(ShieldRow->BlockAngle)).ToString();
					}
				}
				else if (RowStruct == FItemData::StaticStruct())
				{
					if (const FItemData* GeneralRow = ItemDataTable->FindRow<FItemData>(Item.ItemRow.RowName, TEXT("ItemInfoPopup")))
					{
						if (!GeneralRow->BonusStatType.IsEmpty())
						{
							const FText BonusStatFormat = FText::FromString(TEXT("{0}: {1}"));
							StatText = FText::Format(BonusStatFormat,
								FText::FromString(GeneralRow->BonusStatType),
								FText::AsNumber(GeneralRow->BonusStatValue)).ToString();
						}
					}
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("[ItemInfo] RowName=%s DataTable=%s StatText='%s'"),
				*Item.ItemRow.RowName.ToString(),
				Item.ItemRow.DataTable ? *Item.ItemRow.DataTable->GetName() : TEXT("null"),
				* StatText);
			ActiveInfoPopup->SetupInfo(ViewData.DisplayName, ViewData.Grade, TEXT(""),
				ViewData.bIsRelic, ViewData.BenefitDescription, ViewData.RiskDescription,
				ViewData.SellValue, IconTexture, StatText);
			ActiveInfoPopup->AddToViewport(100);
			ActiveInfoPopup->SetKeyboardFocus();
			const FVector2D InfoViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
			ActiveInfoPopup->SetPositionInViewport(InfoViewportPosition, false);
		});
	ActiveContextMenu->AddToViewport(100);
	ActiveContextMenu->SetKeyboardFocus();
	const FVector2D ViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
	ActiveContextMenu->SetPositionInViewport(ViewportPosition + FVector2D(0.f, 4.f), false);
	return true;
}

void UInventoryWidget::ShowSplitPopupForDrop(FGuid InInstanceId, FIntPoint InTargetCell, FVector2D InScreenPosition)
{
	if (BoundInventory == nullptr || ItemSplitPopupClass == nullptr)
	{
		return;
	}
	const int32 EntryIndex = BoundInventory->GetEntries().IndexOfByPredicate([InInstanceId](const FInventoryEntry& Entry)
		{
			return Entry.Item.InstanceId == InInstanceId;
		});
	if (EntryIndex == INDEX_NONE)
	{
		return;
	}
	const FItemInstance Item = BoundInventory->GetEntries()[EntryIndex].Item;
	if (Item.StackCount <= 1)
	{
		return;
	}
	if (ActiveSplitPopup != nullptr)
	{
		ActiveSplitPopup->RemoveFromParent();
		ActiveSplitPopup = nullptr;
	}
	ActiveSplitPopup = CreateWidget<UItemSplitPopupWidget>(GetOwningPlayer(), ItemSplitPopupClass);
	if (ActiveSplitPopup == nullptr)
	{
		return;
	}
	ActiveSplitPopup->SetupRange(1, Item.StackCount - 1);
	ActiveSplitPopup->OnSplitConfirmed.AddLambda([this, InInstanceId, InTargetCell](int32 Quantity)
		{
			if (BoundInventory == nullptr)
			{
				return;
			}
			BoundInventory->Server_SplitStack(InInstanceId, Quantity, InTargetCell);
		});
	ActiveSplitPopup->OnPopupClosed.AddLambda([this]()
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		});
	ActiveSplitPopup->AddToViewport(100);
	ActiveSplitPopup->SetKeyboardFocus();
	const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D LocalPosition = ViewportGeometry.AbsoluteToLocal(InScreenPosition);
	ActiveSplitPopup->SetPositionInViewport(LocalPosition, false);
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
		bHasValidPress = false;
		return FReply::Unhandled();
	}
	if (BoundInventory->GetEntryIndexAt(Cell) == INDEX_NONE)
	{
		bHasValidPress = false;
		return FReply::Unhandled();
	}
	PressedCell = Cell;
	bPressedWithCtrl = InMouseEvent.IsControlDown();
	bHasValidPress = true;
	ScreenToLocal(InMouseEvent.GetScreenSpacePosition(), PressedLocalPosition);
	PressedScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}
	// 우클릭이 유효한 아이템 칸에서 시작된 게 아니면(인벤토리 바깥, 빈 칸 등) 무시.
	if (!bHasValidPress)
	{
		return FReply::Unhandled();
	}
	bHasValidPress = false;
	if (bPressedWithCtrl)
	{
		return HandleContextMenu(PressedCell) ? FReply::Handled() : FReply::Unhandled();
	}
	return HandleRightClick(PressedCell) ? FReply::Handled() : FReply::Unhandled();
}

bool UInventoryWidget::HandleDoubleClick(FIntPoint InCell)
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
	BoundInventory->Server_UseItem(Item.InstanceId);
	return true;
}

bool UInventoryWidget::HandleQuickSlotRegister(int32 InSlotIndex)
{
	if (BoundInventory == nullptr)
	{
		return false;
	}

	const FVector2D CursorScreenPosition = FSlateApplication::Get().GetCursorPos();
	FIntPoint CursorCell;
	if (!ScreenToCell(CursorScreenPosition, CursorCell))
	{
		return false;
	}

	const int32 EntryIndex = BoundInventory->GetEntryIndexAt(CursorCell);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance Item = BoundInventory->GetEntries()[EntryIndex].Item;

	FItemViewData ViewData;
	const bool bResolved = ResolveItemViewData(Item.ItemRow, ViewData);
	if (!bResolved || !ViewData.IsConsumable())
	{
		return false;
	}

	APlayerController* OwningController = GetOwningPlayer();
	if (OwningController == nullptr)
	{
		return false;
	}

	AHB_PlayerState* HBPlayerState = OwningController->GetPlayerState<AHB_PlayerState>();
	if (HBPlayerState == nullptr)
	{
		return false;
	}

	UQuickSlotComponent* QuickSlotComp = HBPlayerState->GetQuickSlot();
	if (QuickSlotComp == nullptr)
	{
		return false;
	}

	QuickSlotComp->Server_SetSlot(InSlotIndex, Item.InstanceId);
	return true;
}

FReply UInventoryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	FIntPoint Cell;
	if (!ScreenToCell(InMouseEvent.GetScreenSpacePosition(), Cell))
	{
		return FReply::Unhandled();
	}
	if (!HandleDoubleClick(Cell))
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
	const FVector2D ItemOrigin(Entry.Position.X * CellSize, (Entry.Position.Y - ScrollOffsetRow) * CellSize);
	const FVector2D PixelOffset = PressedLocalPosition - ItemOrigin;
	UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InstanceId = Entry.Item.InstanceId;
	Operation->GrabOffset = PressedCell - Entry.Position;
	Operation->GrabPixelOffset = PixelOffset;
	Operation->SourceInventory = BoundInventory;
	Operation->bSplitRequested = bPressedWithCtrl && Entry.Item.StackCount > 1;

	// 드래그 아이콘은 이 Operation이 직접 뷰포트에 붙여서 스스로 관리한다.
	// (UInventoryWidget 인스턴스가 아니라 Operation이 소유하므로,
	//  인벤토리 칸/패널 밖으로 나가도, 패널을 넘나들어도 계속 잘 따라다니고
	//  드롭/취소 시 정확히 한 번 정리된다.)
	Operation->CreateVisual(this, IconTexture, VisualSize);
	OutOperation = Operation;
}

void UInventoryWidget::RefocusAfterDrop()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	// SetInputMode를 직접 부르면 CommonUI가 적용해둔 InputConfig를 덮어써서
	// 스택 상태와 입력 모드가 어긋난다. 포커스만 되돌린다.
	TSharedRef<SWidget> WidgetToFocus = (LinkedContainer != nullptr && ParentContainerWidget != nullptr)
		? ParentContainerWidget->TakeWidget()
		: TakeWidget();

	FSlateApplication::Get().SetUserFocus(
		PC->GetLocalPlayer()->GetControllerId(), WidgetToFocus, EFocusCause::SetDirectly);
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearPlacementPreview();
	// 드래그 아이콘 정리는 Operation의 OnDrop 이벤트가 알아서 정확히 한 번 처리한다.
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
	FIntPoint TargetPosition;
	if (Operation->SourceInventory == BoundInventory)
	{
		const FVector2D ItemOrigin = LocalPosition - Operation->GrabPixelOffset;
		TargetPosition = FIntPoint(
			FMath::RoundToInt(ItemOrigin.X / CellSize),
			FMath::RoundToInt(ItemOrigin.Y / CellSize) + ScrollOffsetRow);
	}
	else
	{
		const FIntPoint DropCell(
			FMath::FloorToInt(LocalPosition.X / CellSize),
			FMath::FloorToInt(LocalPosition.Y / CellSize) + ScrollOffsetRow);
		TargetPosition = DropCell - Operation->GrabOffset;
	}
	if (Operation->bSplitRequested && ItemSplitPopupClass != nullptr)
	{
		ShowSplitPopupForDrop(Operation->InstanceId, TargetPosition, InDragDropEvent.GetScreenSpacePosition());
		return true;
	}
	UInventoryComponent* RequestSender = OwningInventory != nullptr ? OwningInventory : BoundInventory;
	if (Operation->SourceEquipment != nullptr)
	{
		RequestSender->Server_UnequipItemTo(Operation->SourceEquipment, Operation->SourceSlot, BoundInventory, TargetPosition);
		RefocusAfterDrop();
		return true;
	}

	const int32 TargetEntryIndex = BoundInventory->GetEntryIndexAt(TargetPosition);
	if (TargetEntryIndex != INDEX_NONE)
	{
		const FGuid TargetInstanceId = BoundInventory->GetEntries()[TargetEntryIndex].Item.InstanceId;
		if (TargetInstanceId != Operation->InstanceId)
		{
			if (Operation->SourceInventory == BoundInventory)
			{
				BoundInventory->Server_MergeStack(Operation->InstanceId, TargetInstanceId);
			}
			else
			{
				BoundInventory->Server_MergeStackFrom(Operation->SourceInventory, Operation->InstanceId, TargetInstanceId);
			}
			RefocusAfterDrop();
			return true;
		}
	}

	if (Operation->SourceInventory == BoundInventory)
	{
		if (BoundInventory == RequestSender)
		{
			RequestSender->Server_MoveItem(Operation->InstanceId, TargetPosition);
		}
		else
		{
			RequestSender->Server_MoveItemIn(BoundInventory, Operation->InstanceId, TargetPosition);
		}
		RefocusAfterDrop();
		return true;
	}
	if (Operation->SourceInventory == RequestSender)
	{
		RequestSender->Server_TransferItem(BoundInventory, Operation->InstanceId, TargetPosition);
	}
	else
	{
		RequestSender->Server_TransferItemFrom(Operation->SourceInventory, Operation->InstanceId, TargetPosition);
	}
	RefocusAfterDrop();
	return true;
}
