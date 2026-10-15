#include "Inventory/OpenedBagWindowWidget.h"
#include "Inventory/InventoryWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Item/Item.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UOpenedBagWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton != nullptr)
	{
		CloseButton->SetClickMethod(EButtonClickMethod::MouseDown);
		CloseButton->OnClicked.AddUniqueDynamic(this, &UOpenedBagWindowWidget::HandleCloseClicked);
	}
}

void UOpenedBagWindowWidget::NativeDestruct()
{
	if (Equipment != nullptr)
	{
		Equipment->OnOpenedBagChanged.RemoveDynamic(this, &UOpenedBagWindowWidget::HandleOpenedBagChanged);
	}
	bDraggingWindow = false;
	Super::NativeDestruct();
}

void UOpenedBagWindowWidget::Setup(UEquipmentComponent* InEquipment, UInventoryComponent* InOwningInventory)
{
	if (Equipment != nullptr)
	{
		Equipment->OnOpenedBagChanged.RemoveDynamic(this, &UOpenedBagWindowWidget::HandleOpenedBagChanged);
	}

	Equipment = InEquipment;
	OwningInventory = InOwningInventory;

	if (Equipment != nullptr)
	{
		Equipment->OnOpenedBagChanged.AddUniqueDynamic(this, &UOpenedBagWindowWidget::HandleOpenedBagChanged);
	}

	Refresh();
}

void UOpenedBagWindowWidget::HandleOpenedBagChanged()
{
	Refresh();
}

void UOpenedBagWindowWidget::HandleCloseClicked()
{
	CloseFromUser();
}

void UOpenedBagWindowWidget::SetOpener(UInventoryWidget* InOpener)
{
	Opener = InOpener;
}

void UOpenedBagWindowWidget::CloseFromUser()
{
	UInventoryWidget* FocusTarget = Opener.Get();
	CloseWindow();
	if (FocusTarget != nullptr && FocusTarget->IsVisible())
	{
		FocusTarget->SetKeyboardFocus();
	}
}

FReply UOpenedBagWindowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseFromUser();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOpenedBagWindowWidget::CloseWindow()
{
	if (Equipment != nullptr && Equipment->HasOpenedBag())
	{
		Equipment->Server_CloseOpenedBag();
	}
	RemoveFromParent();
}

void UOpenedBagWindowWidget::Refresh()
{
	UInventoryComponent* OpenedInventory = Equipment != nullptr ? Equipment->GetOpenedBagInventory() : nullptr;
	if (OpenedInventory == nullptr || !Equipment->HasOpenedBag())
	{
		RemoveFromParent();
		return;
	}

	if (BagPanel != nullptr)
	{
		BagPanel->BindToInventory(OpenedInventory);
		BagPanel->SetOwningInventory(OwningInventory);
		BagPanel->SetLinkedEquipment(Equipment);
		BagPanel->SetEquipmentPanelVisible(false);
	}

	FItemViewData BagView;
	ResolveItemViewData(Equipment->GetOpenedBagRow(), BagView);

	if (TitleText != nullptr)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s (%d x %d)"),
			*BagView.DisplayName, BagView.BagGridSize.X, BagView.BagGridSize.Y)));
	}

	if (Icon != nullptr)
	{
		if (BagView.Icon != nullptr)
		{
			Icon->SetBrushFromTexture(BagView.Icon);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UOpenedBagWindowWidget::SetWindowPosition(FVector2D InPosition)
{
	WindowPosition = InPosition;
	SetPositionInViewport(WindowPosition, false);
}

void UOpenedBagWindowWidget::PlaceNear(FVector2D InViewportPosition)
{
	ForceLayoutPrepass();
	const FVector2D WindowSize = GetDesiredSize();
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;

	FVector2D Position = InViewportPosition + FVector2D(16.f, -24.f);
	Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, ViewportSize.X - WindowSize.X));
	Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - WindowSize.Y));
	SetWindowPosition(Position);
}

FReply UOpenedBagWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bOnTitleBar = TitleBar == nullptr || TitleBar->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
		if (bOnTitleBar)
		{
			bDraggingWindow = true;
			DragStartMouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
			DragStartWindow = WindowPosition;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return FReply::Handled();
}

FReply UOpenedBagWindowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UOpenedBagWindowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow)
	{
		const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
		SetWindowPosition(DragStartWindow + (MousePosition - DragStartMouse));
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
