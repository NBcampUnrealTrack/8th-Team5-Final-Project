#include "InventoryUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "InventoryComponent.h"
#include "InventoryWidget.h"
#include "Player/HBPlayerCharacter.h"

UInventoryUIComponent::UInventoryUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryUIComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureWidget();
}

void UInventoryUIComponent::EnsureWidget()
{
	if (InventoryWidget != nullptr || InventoryWidgetClass == nullptr)
	{
		return;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (OwningController == nullptr || !OwningController->IsLocalController())
	{
		return;
	}

	InventoryWidget = CreateWidget<UInventoryWidget>(OwningController, InventoryWidgetClass);
	if (InventoryWidget == nullptr)
	{
		return;
	}

	InventoryWidget->AddToViewport();
	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryUIComponent::ToggleInventory()
{
	if (bIsOpen)
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

void UInventoryUIComponent::OpenInventory()
{
	EnsureWidget();

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (InventoryWidget == nullptr || OwningController == nullptr)
	{
		return;
	}

	AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(OwningController->GetPawn());
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	InventoryWidget->BindToInventory(PlayerCharacter->GetInventory());
	InventoryWidget->SetVisibility(ESlateVisibility::Visible);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);

	bIsOpen = true;
}

void UInventoryUIComponent::CloseInventory()
{
	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (InventoryWidget == nullptr || OwningController == nullptr)
	{
		return;
	}

	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);

	OwningController->SetInputMode(FInputModeGameOnly());
	OwningController->SetShowMouseCursor(false);

	bIsOpen = false;
}