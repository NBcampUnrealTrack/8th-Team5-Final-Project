#include "InventoryUIComponent.h"

#include "StashComponent.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "InventoryComponent.h"
#include "InventoryWidget.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "UI/HB_UILayout.h"

UInventoryUIComponent::UInventoryUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryUIComponent::BeginPlay()
{
	Super::BeginPlay();

	// 위젯은 열 때 스택이 만든다. 미리 만들어 뷰포트에 붙여두지 않는다.
}

UHB_UILayout* UInventoryUIComponent::GetUILayout() const
{
	const AHBPlayerController* OwningController = Cast<AHBPlayerController>(GetOwner());
	return OwningController ? OwningController->GetUILayout() : nullptr;
}

void UInventoryUIComponent::ToggleInventory()
{
	if (IsInventoryOpen())
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
	if (IsInventoryOpen() || InventoryWidgetClass == nullptr)
	{
		return;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (OwningController == nullptr || !OwningController->IsLocalController())
	{
		return;
	}

	AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(OwningController->GetPawn());
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	UHB_UILayout* Layout = GetUILayout();
	if (Layout == nullptr)
	{
		return;
	}

	// 스택이 위젯을 만들고, 활성화하고, InputConfig(Menu)까지 적용한다.
	// 초기화 함수는 활성화 이전에 실행되므로 첫 프레임부터 데이터가 채워져 있다.
	InventoryWidget = Layout->PushWidgetToLayer<UInventoryWidget>(
		UITag::TAG_UI_Layer_Menu, InventoryWidgetClass,
		[PlayerCharacter](UInventoryWidget& Screen)
		{
			Screen.BindToInventory(PlayerCharacter->GetInventory());
			Screen.BindToEquipment(PlayerCharacter->GetEquipment());
			Screen.SetLinkedEquipment(PlayerCharacter->GetEquipment());
			Screen.SetLinkedContainer(nullptr);
		});
}

void UInventoryUIComponent::CloseInventory()
{
	if (InventoryWidget == nullptr)
	{
		return;
	}

	// 스택이 제거하고, 바로 아래 위젯의 InputConfig를 다시 계산해 적용한다.
	// (창고가 아직 열려 있으면 Menu 유지, 아무것도 없으면 HUD의 Game으로 복귀)
	InventoryWidget->DeactivateWidget();
	InventoryWidget = nullptr;
}

void UInventoryUIComponent::OpenStash()
{
	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (OwningController == nullptr || StashWidgetClass == nullptr)
	{
		return;
	}

	if (StashWidget == nullptr)
	{
		StashWidget = CreateWidget<UUserWidget>(OwningController, StashWidgetClass);
		if (StashWidget == nullptr)
		{
			return;
		}

		StashWidget->AddToViewport();
	}

	StashWidget->SetVisibility(ESlateVisibility::Visible);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(StashWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);
}

void UInventoryUIComponent::CloseStash()
{
	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (StashWidget == nullptr || OwningController == nullptr)
	{
		return;
	}

	if (AHB_PlayerState* HBPlayerState = OwningController->GetPlayerState<AHB_PlayerState>())
	{
		if (UStashComponent* Stash = HBPlayerState->GetStash())
		{
			Stash->RequestSave();
		}
	}

	StashWidget->SetVisibility(ESlateVisibility::Collapsed);

	OwningController->SetInputMode(FInputModeGameOnly());
	OwningController->SetShowMouseCursor(false);
}