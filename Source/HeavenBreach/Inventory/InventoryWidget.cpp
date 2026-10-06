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
	#include "EquipmentSlotWidget.h"
	#include "Item/ItemActionMenuWidget.h"
	#include "Item/Item.h"
	#include "Blueprint/WidgetLayoutLibrary.h"
	#include "Item/UItemSplitPopupWidget.h"
	#include "Blueprint/WidgetBlueprintLibrary.h"
	#include "Item/ItemInfoPopupWidget.h"
	#include "Item/ItemStatBinding.h"
	#include "Framework/Application/SlateApplication.h"
	#include "Player/HBPlayerController.h"
	#include "ShopComponent.h"
	#include "Inventory/QuickSlotComponent.h"
	#include "Widgets/Images/SImage.h"
	#include "Widgets/Layout/SBox.h"
	#include "Engine/GameViewportClient.h"
	#include "Player/PlayerState/HB_PlayerState.h"
	#include "TimerManager.h"
	#include "Components/SizeBox.h"
	#include "Inventory/OpenedBagWindowWidget.h"

	UInventoryWidget::UInventoryWidget()
	{
		// 활성 상태에서 게임 입력을 차단하고 커서를 띄운다.
		// 드래그&드롭 때문에 마우스는 캡처하지 않는다.
		InputMode = EHBWidgetInputMode::Menu;

		// ESC 로 자기 자신을 닫는다. 스택이 아래 위젯을 자동 재활성한다.
		bIsBackHandler = true;
	}

	#pragma region InventoryDesignerPreview
	void UInventoryWidget::NativePreConstruct()
	{
		Super::NativePreConstruct();
	#if WITH_EDITOR
		RefreshDesignerGrid();
	#endif
	}

	void UInventoryWidget::SynchronizeProperties()
	{
		Super::SynchronizeProperties();
	#if WITH_EDITOR
		RefreshDesignerGrid();
	#endif
	}

	#if WITH_EDITOR
	void UInventoryWidget::RefreshDesignerGrid()
	{
		if (!IsDesignTime() || GridCanvas == nullptr || WidgetTree == nullptr || IsTemplate())
		{
			return;
		}

		// Only populate the designer instance; keep the template and gameplay arrays untouched.
		GridCanvas->ClearChildren();
		if (!bShowDesignerGrid || !FMath::IsFinite(CellSize) || CellSize <= 2.0f)
		{
			return;
		}

		const int32 Columns = FMath::Clamp(PreviewColumns, 1, 32);
		const int32 Rows = FMath::Clamp(PreviewRows, 1, 32);
		const int32 DisplayRows = VisibleRows > 0 ? FMath::Min(Rows, VisibleRows) : Rows;
		for (int32 Y = 0; Y < DisplayRows; ++Y)
		{
			for (int32 X = 0; X < Columns; ++X)
			{
				UWidget* PreviewCell = nullptr;
				if (InventoryCellWidgetClass != nullptr && !InventoryCellWidgetClass->HasAnyClassFlags(CLASS_Abstract))
				{
					// WidgetTree ownership does not require a gameplay PlayerController.
					UInventoryCellWidget* CellWidget = WidgetTree->ConstructWidget<UInventoryCellWidget>(InventoryCellWidgetClass);
					if (CellWidget != nullptr)
					{
						CellWidget->SetDesignerFlags(GetDesignerFlags());
						CellWidget->InitializeCell(FIntPoint(X, Y));
						PreviewCell = CellWidget;
					}
				}
				if (PreviewCell == nullptr)
				{
					UBorder* Cell = WidgetTree->ConstructWidget<UBorder>();
					Cell->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.9f));
					Cell->SetPadding(FMargin(0.0f));
					PreviewCell = Cell;
				}

				PreviewCell->SetFlags(RF_Transient);
				PreviewCell->SetVisibility(ESlateVisibility::HitTestInvisible);
				UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(PreviewCell);
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetPosition(FVector2D(X * CellSize, Y * CellSize));
				CanvasSlot->SetSize(FVector2D(CellSize - 2.0f, CellSize - 2.0f));
				CanvasSlot->SetZOrder(0);
			}
		}
		GridCanvas->InvalidateLayoutAndVolatility();
	}
	#endif
	#pragma endregion

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
		RequestCloseOpenedBag();
		ClearHoveredCell();
		ClearHoverInfoPopup();
		ClearPlacementPreview();

		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
		{
			bSuppressDropOnCancel = true;
			FSlateApplication::Get().CancelDragDrop();
			bSuppressDropOnCancel = false;
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
		if (BagSourceEquipment != nullptr)
		{
			BagSourceEquipment->OnEquipmentChanged.RemoveDynamic(this, &UInventoryWidget::HandleBagEquipmentChanged);
		}
		if (OpenedBagEquipment != nullptr)
		{
			RequestCloseOpenedBag();
			OpenedBagEquipment->OnOpenedBagChanged.RemoveDynamic(this, &UInventoryWidget::HandleOpenedBagChanged);
		}
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
		{
			bSuppressDropOnCancel = true;
			FSlateApplication::Get().CancelDragDrop();
			bSuppressDropOnCancel = false;
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

		if (InKeyEvent.GetKey() == EKeys::G)
		{
			if (BoundInventory != nullptr && HoveredItemInstanceId.IsValid())
			{
				if (OwningInventory != nullptr)
				{
					OwningInventory->Server_DropItemFrom(BoundInventory, HoveredItemInstanceId);
				}
				else
				{
					BoundInventory->Server_DropItem(HoveredItemInstanceId);
				}
				return FReply::Handled();
			}

			if (BoundInventory != nullptr && EquipmentPanel != nullptr)
			{
				if (UEquipmentSlotWidget* HoveredSlot = EquipmentPanel->GetHoveredSlotWidget())
				{
					if (HoveredSlot->BoundEquipment != nullptr
						&& HoveredSlot->BoundEquipment->IsSlotOccupied(HoveredSlot->SlotType))
					{
						BoundInventory->Server_DropEquippedItem(HoveredSlot->BoundEquipment, HoveredSlot->SlotType);
						return FReply::Handled();
					}
				}
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

		// R키는 방어 액션과 겹쳐서 임시로 T키를 회전 단축키로 사용한다.
		// ActiveDragOperation은 인벤토리 칸에서 드래그를 "시작"한 경우에만 채워진다.
		// 장비 슬롯(UEquipmentSlotWidget)에서 드래그를 시작하면 이게 비어있어서
		// T키가 안 먹는 버그가 있었다. 그래서 어디서 드래그가 시작됐든 상관없이
		// 현재 진행 중인 드래그 오퍼레이션을 직접 가져오는 방식으로 바꾼다.
		if (InKeyEvent.GetKey() == EKeys::T)
		{
			if (UInventoryDragDropOperation* DragOperation =
				Cast<UInventoryDragDropOperation>(UWidgetBlueprintLibrary::GetDragDroppingContent()))
			{
				// 장비 슬롯에서 드래그해서 빼는 중에는 회전을 지원하지 않는다.
				// EquipmentComponent::EquipFromInventory에서 장착하는 순간 항상
				// bRotated = false로 초기화하기 때문에, 굳이 빼는 도중에 돌릴 수 있게
				// 열어두면 오히려 혼란만 준다. 인벤토리 칸에서 시작된 드래그에서만
				// T 회전을 허용한다.
				if (DragOperation->SourceEquipment == nullptr)
				{
					DragOperation->SetRotated(!DragOperation->bRotated);
					UpdatePlacementPreviewAt(FSlateApplication::Get().GetCursorPos(), DragOperation);
					return FReply::Handled();
				}
			}
		}

		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	void UInventoryWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
	{
		Super::NativeOnFocusLost(InFocusEvent);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UInventoryWidget::TryReclaimFocus);
		}
	}

	void UInventoryWidget::TryReclaimFocus()
	{
		if (!IsVisible())
		{
			return;
		}
		if (HasAnyUserFocus() || HasFocusedDescendants())
		{
			return;
		}
		if (!ShouldReclaimFocus())
		{
			return;
		}
		SetKeyboardFocus();
	}

	bool UInventoryWidget::ShouldReclaimFocus() const
	{
		if (LinkedContainer != nullptr)
		{
			return false;
		}

		if (IsValid(ActiveContextMenu) && ActiveContextMenu->IsInViewport())
		{
			return false;
		}
		if (IsValid(ActiveSplitPopup) && ActiveSplitPopup->IsInViewport())
		{
			return false;
		}
		if (IsValid(ActiveInfoPopup) && ActiveInfoPopup->IsInViewport())
		{
			return false;
		}

		return true;
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
		if (USizeBox* GridSizeBox = Cast<USizeBox>(GridCanvas->GetParent()))
		{
			GridSizeBox->SetWidthOverride(BoundInventory->GetGridSize().X * CellSize);
			GridSizeBox->SetHeightOverride(GetVisibleRowCount() * CellSize);
		}
		BuildBackground();
		BuildItems();
	}

	void UInventoryWidget::BindToEquipment(UEquipmentComponent* InEquipment)
	{
		if (EquipmentPanel != nullptr)
		{
			EquipmentPanel->BindToEquipment(InEquipment, BoundInventory, this);
		}

		if (BagPanel != nullptr)
		{
			if (BagSourceEquipment != nullptr)
			{
				BagSourceEquipment->OnEquipmentChanged.RemoveDynamic(this, &UInventoryWidget::HandleBagEquipmentChanged);
			}
			BagSourceEquipment = InEquipment;
			if (BagSourceEquipment != nullptr)
			{
				BagSourceEquipment->OnEquipmentChanged.AddUniqueDynamic(this, &UInventoryWidget::HandleBagEquipmentChanged);
			}
			RefreshBagPanel();
		}

		if (OpenedBagEquipment != nullptr)
		{
			OpenedBagEquipment->OnOpenedBagChanged.RemoveDynamic(this, &UInventoryWidget::HandleOpenedBagChanged);
		}
		OpenedBagEquipment = InEquipment;
		if (OpenedBagEquipment != nullptr)
		{
			OpenedBagEquipment->OnOpenedBagChanged.AddUniqueDynamic(this, &UInventoryWidget::HandleOpenedBagChanged);
		}
	}

	void UInventoryWidget::HandleOpenedBagChanged()
	{
		EnsureOpenedBagWindow();
	}

	UOpenedBagWindowWidget* UInventoryWidget::FindOpenedBagWindow() const
	{
		UWorld* World = GetWorld();
		if (World == nullptr)
		{
			return nullptr;
		}

		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, UOpenedBagWindowWidget::StaticClass(), true);
		for (UUserWidget* Found : FoundWidgets)
		{
			if (Found != nullptr && Found->GetOwningPlayer() == GetOwningPlayer())
			{
				return Cast<UOpenedBagWindowWidget>(Found);
			}
		}
		return nullptr;
	}

	void UInventoryWidget::EnsureOpenedBagWindow()
	{
		if (OpenedBagEquipment == nullptr || !OpenedBagEquipment->HasOpenedBag())
		{
			return;
		}

		if (OpenedBagWindowClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OpenedBag] OpenedBagWindowClass is not set on %s"), *GetName());
			return;
		}

		if (FindOpenedBagWindow() != nullptr)
		{
			return;
		}

		UOpenedBagWindowWidget* Window = CreateWidget<UOpenedBagWindowWidget>(GetOwningPlayer(), OpenedBagWindowClass);
		if (Window == nullptr)
		{
			return;
		}

		Window->AddToViewport(50);
		Window->Setup(OpenedBagEquipment, BoundInventory);
		Window->PlaceNear(UWidgetLayoutLibrary::GetMousePositionOnViewport(this));
	}

	void UInventoryWidget::RequestCloseOpenedBag()
	{
		if (UOpenedBagWindowWidget* Window = FindOpenedBagWindow())
		{
			Window->CloseWindow();
		}
		if (OpenedBagEquipment != nullptr && OpenedBagEquipment->HasOpenedBag())
		{
			OpenedBagEquipment->Server_CloseOpenedBag();
		}
	}

	UInventoryComponent* UInventoryWidget::FindOpenedBagInventory() const
	{
		const AHB_PlayerState* OwnerPlayerState = GetOwningPlayerState<AHB_PlayerState>();
		const UEquipmentComponent* OwnerEquipment = OwnerPlayerState != nullptr ? OwnerPlayerState->GetEquipment() : nullptr;
		if (OwnerEquipment == nullptr || !OwnerEquipment->HasOpenedBag())
		{
			return nullptr;
		}
		return OwnerEquipment->GetOpenedBagInventory();
	}

	bool UInventoryWidget::TryOpenBagItem(const FItemInstance& InItem)
	{
		if (BoundInventory == nullptr || BoundInventory->IsBagStorage() || !InItem.bRevealed)
		{
			return false;
		}

		FItemViewData ViewData;
		if (!ResolveItemViewData(InItem.ItemRow, ViewData) || !ViewData.IsBag())
		{
			return false;
		}

		AHB_PlayerState* OwnerPlayerState = GetOwningPlayerState<AHB_PlayerState>();
		UEquipmentComponent* OwnerEquipment = OwnerPlayerState != nullptr ? OwnerPlayerState->GetEquipment() : nullptr;
		if (OwnerEquipment == nullptr)
		{
			return false;
		}

		OwnerEquipment->Server_OpenBag(BoundInventory, InItem.InstanceId);
		return true;
	}

	void UInventoryWidget::HandleBagEquipmentChanged()
	{
		RefreshBagPanel();
	}

	void UInventoryWidget::RefreshBagPanel()
	{
		if (BagPanel == nullptr)
		{
			return;
		}

		UInventoryComponent* BagInventory = BagSourceEquipment != nullptr ? BagSourceEquipment->GetBagInventory() : nullptr;
		FItemInstance BagItem;
		const bool bHasBag = BagInventory != nullptr && BagSourceEquipment->GetItemInSlot(EEquipSlotType::Backpack, BagItem);
		if (!bHasBag)
		{
			BagPanel->SetVisibility(ESlateVisibility::Collapsed);
			if (BagSection != nullptr)
			{
				BagSection->SetVisibility(ESlateVisibility::Collapsed);
			}
			return;
		}

		BagPanel->BindToInventory(BagInventory);
		BagPanel->SetOwningInventory(BoundInventory);
		BagPanel->SetLinkedEquipment(BagSourceEquipment);
		BagPanel->SetEquipmentPanelVisible(false);
		BagPanel->SetVisibility(ESlateVisibility::Visible);

		if (BagSection != nullptr)
		{
			BagSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		FItemViewData BagView;
		ResolveItemViewData(BagItem.ItemRow, BagView);

		if (BagTitleText != nullptr)
		{
			BagTitleText->SetText(FText::FromString(FString::Printf(TEXT("%s (%d x %d)"),
				*BagView.DisplayName, BagView.BagGridSize.X, BagView.BagGridSize.Y)));
		}

		if (BagIcon != nullptr)
		{
			if (BagView.Icon != nullptr)
			{
				BagIcon->SetBrushFromTexture(BagView.Icon);
				BagIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				BagIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
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

		if (LocalPosition.X < 0.0f || LocalPosition.Y < 0.0f)
		{
			return false;
		}

		const int32 LocalRow = FMath::FloorToInt(LocalPosition.Y / CellSize);

		if (LocalRow >= GetVisibleRowCount())
		{
			return false;
		}

		const FIntPoint Cell(
			FMath::FloorToInt(LocalPosition.X / CellSize),
			LocalRow + ScrollOffsetRow);

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
			UpdateHoverInfoPopup(Cell);
		}
		else
		{
			ClearHoveredCell();
			ClearHoverInfoPopup();
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
		ClearHoverInfoPopup();
		Super::NativeOnMouseLeave(InMouseEvent);
	}

	bool UInventoryWidget::UpdatePlacementPreview(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
	{
		return UpdatePlacementPreviewAt(InDragDropEvent.GetScreenSpacePosition(), InOperation);
	}

	bool UInventoryWidget::UpdatePlacementPreviewAt(const FVector2D& InScreenSpacePosition, UDragDropOperation* InOperation)
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

		// 드래그 중 T키로 토글한 회전 상태를 미리보기 크기/충돌 계산에 반영한다.
		DraggedItem.bRotated = Operation->bRotated;

		FItemViewData ViewData;
		if (!ResolveItemViewData(DraggedItem.ItemRow, ViewData))
		{
			ClearPlacementPreview();
			return false;
		}

		FVector2D LocalPosition;
		if (!ScreenToLocal(InScreenSpacePosition, LocalPosition))
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

		const bool bCanPlace = BoundInventory->AcceptsItem(DraggedItem)
			&& BoundInventory->CanPlaceAt(ItemSize, TargetPosition, IgnoredEntryIndex);

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
		ActiveDragOperation.Reset();

		const FVector2D CancelScreenPosition = InDragDropEvent.GetScreenSpacePosition();
		const UOpenedBagWindowWidget* OpenedBagWindow = FindOpenedBagWindow();
		const bool bReleasedOverWindow = GetCachedGeometry().IsUnderLocation(CancelScreenPosition)
			|| (ParentContainerWidget != nullptr && ParentContainerWidget->GetCachedGeometry().IsUnderLocation(CancelScreenPosition))
			|| (OpenedBagWindow != nullptr && OpenedBagWindow->GetCachedGeometry().IsUnderLocation(CancelScreenPosition));

		if (!bSuppressDropOnCancel && OwningInventory == nullptr && !bReleasedOverWindow && GetOwningPlayerPawn() != nullptr)
		{
			if (const UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation))
			{
				if (Operation->SourceEquipment == nullptr
					&& Operation->SourceInventory == BoundInventory
					&& BoundInventory != nullptr)
				{
					BoundInventory->Server_DropItem(Operation->InstanceId);
				}
			}
		}

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
		UInventoryComponent* EquipSource = BoundInventory->IsBagStorage() ? BoundInventory.Get() : RequestSender;
		EquipSource->Server_EquipItem(LinkedEquipment, Item.InstanceId, ViewData.EquipSlotType);
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
			UE_LOG(LogTemp, Warning, TEXT("[ContextMenu] ItemContextMenuClass is not set on %s"), *GetName());
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
		const bool bCanOpenBag = ViewData.IsBag() && !BoundInventory->IsBagStorage() && Item.bRevealed;
		const bool bCanUse = ViewData.IsConsumable() || bCanOpenBag;
		const bool bCanSplit = Item.StackCount > 1;
		ActiveContextMenu->SetupForItem(bCanUse, bCanSplit, Item.bIsFavorite);
		if (bCanOpenBag)
		{
			ActiveContextMenu->SetUseButtonLabel(FText::FromString(TEXT("\uC5F4\uAE30")));
		}
		const FGuid InstanceId = Item.InstanceId;
		ActiveContextMenu->OnUseRequested.AddLambda([this, InstanceId, bCanOpenBag]()
			{
				if (BoundInventory == nullptr)
				{
					return;
				}
				if (bCanOpenBag)
				{
					const int32 BagIndex = BoundInventory->FindEntryIndexById(InstanceId);
					if (BagIndex != INDEX_NONE)
					{
						TryOpenBagItem(BoundInventory->GetEntries()[BagIndex].Item);
					}
					return;
				}
				BoundInventory->Server_UseItem(InstanceId);
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
		ActiveContextMenu->OnCloseRequested.AddLambda([this]()
			{
				if (LinkedContainer == nullptr)
				{
					if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
					{
						PC->RequestCloseInventory();
					}
				}
			});
		ActiveContextMenu->OnMenuClosed.AddLambda([this]()
			{
				ActiveContextMenu = nullptr;
				if (LinkedContainer == nullptr)
				{
					if (UWorld* World = GetWorld())
					{
						World->GetTimerManager().SetTimerForNextTick(this, &UInventoryWidget::TryReclaimFocus);
					}
				}
			});
		ActiveContextMenu->AddToViewport(100);
		ActiveContextMenu->SetKeyboardFocus();
		const FVector2D ViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
		ActiveContextMenu->SetPositionInViewport(ViewportPosition + FVector2D(0.f, 4.f), false);
		return true;
	}

	bool UInventoryWidget::OpenEquippedItemMenu(UEquipmentComponent* InEquipment, EEquipSlotType InSlot)
	{
		if (InEquipment == nullptr || BoundInventory == nullptr || ItemContextMenuClass == nullptr)
		{
			return false;
		}

		FItemInstance EquippedItem;
		if (!InEquipment->GetItemInSlot(InSlot, EquippedItem))
		{
			return false;
		}

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

		ActiveContextMenu->SetupForItem(false, false, EquippedItem.bIsFavorite);
		TWeakObjectPtr<UEquipmentComponent> WeakEquipment = InEquipment;
		ActiveContextMenu->OnDiscardRequested.AddLambda([this, WeakEquipment, InSlot]()
			{
				if (BoundInventory != nullptr && WeakEquipment.IsValid())
				{
					BoundInventory->Server_DropEquippedItem(WeakEquipment.Get(), InSlot);
				}
			});
		ActiveContextMenu->OnFavoriteRequested.AddLambda([this, WeakEquipment, InSlot]()
			{
				if (BoundInventory != nullptr && WeakEquipment.IsValid())
				{
					BoundInventory->Server_ToggleEquippedFavorite(WeakEquipment.Get(), InSlot);
				}
			});
		ActiveContextMenu->OnCloseRequested.AddLambda([this]()
			{
				if (LinkedContainer == nullptr)
				{
					if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
					{
						PC->RequestCloseInventory();
					}
				}
			});
		ActiveContextMenu->OnMenuClosed.AddLambda([this]()
			{
				ActiveContextMenu = nullptr;
				if (LinkedContainer == nullptr)
				{
					if (UWorld* World = GetWorld())
					{
						World->GetTimerManager().SetTimerForNextTick(this, &UInventoryWidget::TryReclaimFocus);
					}
				}
			});
		ActiveContextMenu->AddToViewport(100);
		ActiveContextMenu->SetKeyboardFocus();
		const FVector2D ViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
		ActiveContextMenu->SetPositionInViewport(ViewportPosition + FVector2D(0.f, 4.f), false);
		return true;
	}

	void UInventoryWidget::UpdateHoverInfoPopup(FIntPoint InCell)
	{
		if (BoundInventory == nullptr || ItemInfoPopupClass == nullptr)
		{
			return;
		}

		const int32 EntryIndex = BoundInventory->GetEntryIndexAt(InCell);
		if (EntryIndex == INDEX_NONE)
		{
			ClearHoverInfoPopup();
			return;
		}

		const FItemInstance& Item = BoundInventory->GetEntries()[EntryIndex].Item;

		// 서칭 중(bRevealed=false)인 아이템은 "?"로만 보여야 하므로 정보 팁을 띄우지 않는다.
		if (!Item.bRevealed)
		{
			ClearHoverInfoPopup();
			return;
		}

		// 같은 아이템 위에서 계속 움직이는 중이면 다시 만들지 않고 위치만 따라가게 한다.
		if (HoverInfoPopup != nullptr && HoveredItemInstanceId == Item.InstanceId)
		{
			const FVector2D InfoViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(16.f, 16.f);
			HoverInfoPopup->SetPositionInViewport(InfoViewportPosition, false);
			return;
		}

		FItemViewData ViewData;
		if (!ResolveItemViewData(Item.ItemRow, ViewData))
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

		// 이제 Ctrl+우클릭 전용 상세 팝업이 없어졌으니, 호버만으로도 예전 "상세 정보"와
		// 동일한 내용(기본 효과/각인 효과/내구도/설명 등)을 그대로 보여준다.
		const FString StatText = BuildStatTextForItem(Item);
		const FString EquipSlotLabel = GetEquipSlotDisplayName(ViewData.EquipSlotType);
		// 초록 칸: 좋은 숫자 옵션 + 좋은 각인 + 좋은 동작 효과 설명
		// 빨강 칸: 나쁜 숫자 옵션 + 나쁜 각인 + 나쁜 동작 효과 설명
		// 옛 BenefitDescription / RiskDescription 문자열은 더 안 읽는다. 옵션 배열에서 문장을 만든다.
		const FString BenefitLines = BuildBenefitLinesForItem(Item, ViewData);
		const FString RiskLines = BuildRiskLinesForItem(Item, ViewData);

		HoverInfoPopup->SetupInfo(ViewData.DisplayName, ViewData.Grade, ViewData.Description,
			BenefitLines, RiskLines,
			ViewData.SellValue, IconTexture, StatText, /*bInCompact=*/false,
			EquipSlotLabel, Item.CurrentDurability, GetMaxDurabilityForGrade(ViewData.Grade));

		HoverInfoPopup->AddToViewport(100);
		// 클릭형 정보 팝업과 달리 포커스를 가져가면 안 된다 (마우스 이동만으로 키/드래그 입력이
		// 끊기게 됨). HitTestInvisible로 설정해서 마우스 이벤트가 툴팁을 뚫고 그리드로
		// 그대로 전달되게 한다 — 안 그러면 팝업이 커서 위에 겹칠 때 호버가 깜빡거린다.
		HoverInfoPopup->SetVisibility(ESlateVisibility::HitTestInvisible);

		const FVector2D InfoViewportPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(16.f, 16.f);
		HoverInfoPopup->SetPositionInViewport(InfoViewportPosition, false);

		HoveredItemInstanceId = Item.InstanceId;
	}

	void UInventoryWidget::ClearHoverInfoPopup()
	{
		if (HoverInfoPopup != nullptr)
		{
			HoverInfoPopup->RemoveFromParent();
			HoverInfoPopup = nullptr;
		}
		HoveredItemInstanceId = FGuid();
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
		bPressedWithShift = InMouseEvent.IsShiftDown();
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
		if (bPressedWithShift && HandleQuickMove(PressedCell))
		{
			return FReply::Handled();
		}
		return HandleRightClick(PressedCell) ? FReply::Handled() : FReply::Unhandled();
	}

	UInventoryComponent* UInventoryWidget::ResolveQuickMoveTarget() const
	{
		if (BoundInventory == nullptr)
		{
			return nullptr;
		}
		if (BoundInventory->IsBagStorage() && OwningInventory != nullptr && OwningInventory != BoundInventory)
		{
			return OwningInventory;
		}
		if (UInventoryComponent* OpenedBagInventory = FindOpenedBagInventory())
		{
			if (OpenedBagInventory != BoundInventory)
			{
				return OpenedBagInventory;
			}
		}
		if (BagPanel != nullptr && BagPanel->IsVisible() && BagPanel->BoundInventory != nullptr && BagPanel->BoundInventory != BoundInventory)
		{
			return BagPanel->BoundInventory;
		}
		return nullptr;
	}

	bool UInventoryWidget::HandleQuickMove(FIntPoint InCell)
	{
		UInventoryComponent* Target = ResolveQuickMoveTarget();
		if (Target == nullptr)
		{
			return false;
		}
		const int32 EntryIndex = BoundInventory->GetEntryIndexAt(InCell);
		if (EntryIndex == INDEX_NONE)
		{
			return false;
		}
		const FItemInstance& Item = BoundInventory->GetEntries()[EntryIndex].Item;
		if (!Item.bRevealed || !Target->AcceptsItem(Item))
		{
			return false;
		}
		if (Target->IsBagStorage())
		{
			Target->Server_TransferItemFromAuto(BoundInventory, Item.InstanceId);
		}
		else
		{
			BoundInventory->Server_TransferItemAuto(Target, Item.InstanceId);
		}
		return true;
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
		if (TryOpenBagItem(Item))
		{
			return true;
		}
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
		ClearHoverInfoPopup();
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
		Operation->bRotated = Entry.Item.bRotated;

		// 드래그 아이콘은 이 Operation이 직접 뷰포트에 붙여서 스스로 관리한다.
		// (UInventoryWidget 인스턴스가 아니라 Operation이 소유하므로,
		//  인벤토리 칸/패널 밖으로 나가도, 패널을 넘나들어도 계속 잘 따라다니고
		//  드롭/취소 시 정확히 한 번 정리된다.)
		Operation->CreateVisual(this, IconTexture, VisualSize);
		ActiveDragOperation = Operation;
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
		ActiveDragOperation.Reset();
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
				RequestSender->Server_MoveItem(Operation->InstanceId, TargetPosition, Operation->bRotated);
			}
			else
			{
				RequestSender->Server_MoveItemIn(BoundInventory, Operation->InstanceId, TargetPosition, Operation->bRotated);
			}
			RefocusAfterDrop();
			return true;
		}
		UInventoryComponent* SourceInventory = Operation->SourceInventory;
		if (SourceInventory == RequestSender)
		{
			RequestSender->Server_TransferItem(BoundInventory, Operation->InstanceId, TargetPosition, Operation->bRotated);
		}
		else if (BoundInventory != RequestSender && SourceInventory != nullptr && SourceInventory->IsBagStorage())
		{
			SourceInventory->Server_TransferItem(BoundInventory, Operation->InstanceId, TargetPosition, Operation->bRotated);
		}
		else if (BoundInventory != RequestSender && BoundInventory->IsBagStorage())
		{
			BoundInventory->Server_TransferItemFrom(SourceInventory, Operation->InstanceId, TargetPosition, Operation->bRotated);
		}
		else
		{
			RequestSender->Server_TransferItemFrom(SourceInventory, Operation->InstanceId, TargetPosition, Operation->bRotated);
		}
		RefocusAfterDrop();
		return true;
	}
