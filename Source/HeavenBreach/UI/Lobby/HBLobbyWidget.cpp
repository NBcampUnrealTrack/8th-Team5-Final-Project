#include "UI/Lobby/HBLobbyWidget.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"

#include "UI/Friends/HB_FriendsPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	void SetActionLabel(UUserWidget* Module, const FText& Label)
	{
		if (!Module) return;
		// RefreshVisual reads these Blueprint properties again when the module updates.
		for (const FName PropertyName : {FName(TEXT("ButtonLabel")), FName(TEXT("ButtonTooltip"))})
			if (FTextProperty* Property = FindFProperty<FTextProperty>(Module->GetClass(), PropertyName))
				Property->SetPropertyValue_InContainer(Module, Label);
		if (auto* Text = Cast<UTextBlock>(Module->GetWidgetFromName(TEXT("Text_Label"))))
			Text->SetText(Label);
		Module->SetToolTipText(Label);
	}
}

void UHBLobbyWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshLobbyActions();
}

void UHBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Keep the existing widget names so the assembled Blueprint retains its layout.
	if (StartMatchButtonModule)
	{
		StartMatchButton = Cast<UButton>(StartMatchButtonModule->GetWidgetFromName(TEXT("Button_Action")));
	}
	if (FriendsButtonModule)
	{
		// 모듈 안의 버튼 이름을 모르므로 먼저 다른 모듈과 같은 이름을 시도하고, 없으면 첫 버튼을 쓴다.
		FriendsButton = Cast<UButton>(FriendsButtonModule->GetWidgetFromName(TEXT("Button_Action")));
		if (!FriendsButton && FriendsButtonModule->WidgetTree)
		{
			FriendsButtonModule->WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!FriendsButton)
				{
					FriendsButton = Cast<UButton>(Widget);
				}
			});
		}
	}
	if (FriendsButton)
	{
		FriendsButton->OnClicked.AddUniqueDynamic(this, &UHBLobbyWidget::HandleFriendsClicked);
	}
	else if (FriendsButtonModule)
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyWidget | FriendsButtonModule 안에서 Button 을 찾지 못했다. 친구 목록이 열리지 않는다."));
	}
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
	}
	RefreshLobbyActions();
}

void UHBLobbyWidget::NativeDestruct()
{
	if (FriendsButton)
	{
		FriendsButton->OnClicked.RemoveDynamic(this, &UHBLobbyWidget::HandleFriendsClicked);
	}
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartClicked);
	}
	Super::NativeDestruct();
}

#pragma region LobbyActionRequests
bool UHBLobbyWidget::IsLocalLobbyHost() const
{
	if (IsDesignTime())
	{
		return bPreviewAsHost;
	}
	const auto* Player = GetOwningPlayer();
	const auto* Instance = GetGameInstance();
	const auto* Sessions = Instance ? Instance->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
	return IsValid(Player) && Player->IsLocalController() && Sessions && Sessions->IsLocalLobbyHost();
}

void UHBLobbyWidget::RefreshLobbyActions()
{
	const auto* PlayerState = GetOwningPlayerState<AHBLobbyPlayerState>();
	const auto* Player = GetOwningPlayer();
	const auto* State = GetWorld() ? GetWorld()->GetGameState<AHBLobbyGameState>() : nullptr;
	const auto* Instance = GetGameInstance();
	const auto* Sessions = Instance ? Instance->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
	const bool bHost = IsLocalLobbyHost();
	const bool bInLobby = Player && Player->IsLocalController() && PlayerState && State && State->HostPlayerState
		&& State->PlayerArray.Contains(PlayerState) && Sessions
		&& Sessions->GetLobbyOperationState() == EHBLobbyOperationState::InLobby
		&& !Sessions->IsLobbyOperationInProgress() && !Sessions->IsMatchAdmissionClosed();
	const bool bCanStart = IsDesignTime() ? bHost
		: bInLobby && bHost && State->HostPlayerState == PlayerState && State->CanStartMatch();
	SetActionLabel(StartMatchButtonModule, NSLOCTEXT("LobbyActions", "Start", "시작"));
	if (StartMatchButtonModule) StartMatchButtonModule->SetIsEnabled(bCanStart);
	if (StartMatchButton) StartMatchButton->SetIsEnabled(bCanStart);
	// Keep the designer's widgets, but retire their runtime Ready action.
	if (ReadyButtonModule) ReadyButtonModule->SetVisibility(ESlateVisibility::Collapsed);
	if (ReadyButton) ReadyButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UHBLobbyWidget::HandleFriendsClicked()
{
	if (FriendsPanel)
	{
		FriendsPanel->Toggle();
	}
}

void UHBLobbyWidget::HandleStartClicked()
{
	RefreshLobbyActions();
	if (StartMatchButton && StartMatchButton->GetIsEnabled() && IsLocalLobbyHost())
	{
		OnStartRequested.Broadcast();
	}
}
#pragma endregion
