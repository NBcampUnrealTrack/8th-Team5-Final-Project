#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Audio/HBUIAudioLibrary.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

void UHBLobbyWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshLobbyActions();
}

void UHBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	AudioPlayerState = GetOwningPlayerState<AHBLobbyPlayerState>();
	if (AudioPlayerState) AudioPlayerState->OnReadyStateChanged.AddUniqueDynamic(this, &ThisClass::HandleReadyAudio);

	// Keep the existing widget names so the assembled Blueprint retains its layout.
	if (ReadyButtonModule)
	{
		ReadyButton = Cast<UButton>(ReadyButtonModule->GetWidgetFromName(TEXT("Button_Action")));
	}
	if (StartMatchButtonModule)
	{
		StartMatchButton = Cast<UButton>(StartMatchButtonModule->GetWidgetFromName(TEXT("Button_Action")));
	}
	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddUniqueDynamic(this, &UHBLobbyWidget::HandleFindRoomClicked);
	}
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.AddUniqueDynamic(this, &UHBLobbyWidget::HandlePrimaryActionClicked);
		if (!IsLocalLobbyHost())
		{
			FButtonStyle Style = StartMatchButton->GetStyle();
			Style.PressedSlateSound.SetResourceObject(nullptr);
			StartMatchButton->SetStyle(Style);
		}
	}
	RefreshLobbyActions();
}

void UHBLobbyWidget::NativeDestruct()
{
	if (AudioPlayerState) AudioPlayerState->OnReadyStateChanged.RemoveDynamic(this, &ThisClass::HandleReadyAudio);
	AudioPlayerState = nullptr;
	if (ReadyButton)
	{
		ReadyButton->OnClicked.RemoveDynamic(this, &UHBLobbyWidget::HandleFindRoomClicked);
	}
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.RemoveDynamic(this, &UHBLobbyWidget::HandlePrimaryActionClicked);
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
	const APlayerController* PlayerController = GetOwningPlayer();
	// The listen host owns an authoritative local controller; a joining client does not.
	return IsValid(PlayerController) && PlayerController->IsLocalController() && PlayerController->HasAuthority();
}

void UHBLobbyWidget::RefreshLobbyActions()
{
	if (!StartMatchButtonModule)
	{
		return;
	}
	const FText Label = IsLocalLobbyHost()
		? NSLOCTEXT("LobbyActions", "Start", "시작")
		: NSLOCTEXT("LobbyActions", "Ready", "준비");
	// Keep the module's RefreshVisual from restoring an outdated label.
	for (const FName PropertyName : {FName(TEXT("ButtonLabel")), FName(TEXT("ButtonTooltip"))})
	{
		if (FTextProperty* TextProperty = FindFProperty<FTextProperty>(StartMatchButtonModule->GetClass(), PropertyName))
		{
			TextProperty->SetPropertyValue_InContainer(StartMatchButtonModule, Label);
		}
	}
	if (UTextBlock* Text = Cast<UTextBlock>(StartMatchButtonModule->GetWidgetFromName(TEXT("Text_Label"))))
	{
		Text->SetText(Label);
	}
	StartMatchButtonModule->SetToolTipText(Label);
}

void UHBLobbyWidget::HandleReadyAudio(bool bReady)
{
	UHBUIAudioLibrary::PlayForOwner(this, bReady ? EHBUIAudioEvent::LobbyReady : EHBUIAudioEvent::Cancel);
}

void UHBLobbyWidget::HandleFindRoomClicked()
{
	OnFindRoomRequested.Broadcast();
}

void UHBLobbyWidget::HandlePrimaryActionClicked()
{
	RefreshLobbyActions();
	if (IsLocalLobbyHost())
	{
		OnStartRequested.Broadcast();
	}
	else
	{
		OnReadyRequested.Broadcast();
	}
}
#pragma endregion
