#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Audio/HBUIAudioLibrary.h"
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
	if (ReadyButtonModule)
	{
		ReadyButton = Cast<UButton>(ReadyButtonModule->GetWidgetFromName(TEXT("Button_Action")));
	}
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
	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReadyClicked);
		// The replicated ready state supplies the confirmation/cancel sound.
		FButtonStyle Style = ReadyButton->GetStyle();
		Style.PressedSlateSound.SetResourceObject(nullptr);
		ReadyButton->SetStyle(Style);
	}
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
	}
	RefreshLobbyActions();
}

void UHBLobbyWidget::NativeDestruct()
{
	if (AudioPlayerState) AudioPlayerState->OnReadyStateChanged.RemoveDynamic(this, &ThisClass::HandleReadyAudio);
	AudioPlayerState = nullptr;
	if (FriendsButton)
	{
		FriendsButton->OnClicked.RemoveDynamic(this, &UHBLobbyWidget::HandleFriendsClicked);
	}
	if (ReadyButton)
	{
		ReadyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleReadyClicked);
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
	auto* PlayerState = GetOwningPlayerState<AHBLobbyPlayerState>();
	// A remote PlayerState can arrive after NativeConstruct.
	if (!IsDesignTime() && AudioPlayerState != PlayerState)
	{
		if (AudioPlayerState) AudioPlayerState->OnReadyStateChanged.RemoveDynamic(this, &ThisClass::HandleReadyAudio);
		AudioPlayerState = PlayerState;
		if (AudioPlayerState) AudioPlayerState->OnReadyStateChanged.AddUniqueDynamic(this, &ThisClass::HandleReadyAudio);
	}
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
	const bool bCanReady = IsDesignTime() ? !bHost
		: bInLobby && !bHost && Player->GetNetMode() == NM_Client && State->HostPlayerState != PlayerState
			&& (PlayerState->IsReady() || PlayerState->HasSelectedJob());
	SetActionLabel(StartMatchButtonModule, NSLOCTEXT("LobbyActions", "Start", "시작"));
	SetActionLabel(ReadyButtonModule, PlayerState && PlayerState->IsReady()
		? NSLOCTEXT("LobbyActions", "CancelReady", "준비 취소") : NSLOCTEXT("LobbyActions", "Ready", "준비"));
	if (StartMatchButtonModule) StartMatchButtonModule->SetIsEnabled(bCanStart);
	if (StartMatchButton) StartMatchButton->SetIsEnabled(bCanStart);
	if (ReadyButtonModule) ReadyButtonModule->SetIsEnabled(bCanReady);
	if (ReadyButton) ReadyButton->SetIsEnabled(bCanReady);
}

void UHBLobbyWidget::HandleReadyAudio(bool bReady)
{
	RefreshLobbyActions();
	UHBUIAudioLibrary::PlayForOwner(this, bReady ? EHBUIAudioEvent::LobbyReady : EHBUIAudioEvent::Cancel);
}

void UHBLobbyWidget::HandleFriendsClicked()
{
	if (FriendsPanel)
	{
		FriendsPanel->Toggle();
	}
}

void UHBLobbyWidget::HandleReadyClicked()
{
	RefreshLobbyActions();
	if (ReadyButton && ReadyButton->GetIsEnabled() && !IsLocalLobbyHost())
		OnReadyRequested.Broadcast();
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
