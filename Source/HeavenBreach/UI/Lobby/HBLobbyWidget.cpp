#include "UI/Lobby/HBLobbyWidget.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"

#include "UI/Friends/HB_FriendsPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	UButton* FindModuleButton(UUserWidget* Module, FName PreferredName)
	{
		if (!Module) return nullptr;
		auto* Button = Cast<UButton>(Module->GetWidgetFromName(PreferredName));
		if (!Button && Module->WidgetTree)
			Module->WidgetTree->ForEachWidget([&](UWidget* Widget)
			{
				if (!Button) Button = Cast<UButton>(Widget);
			});
		return Button;
	}

	void RefreshLobbyNameTags(const AHBLobbyGameState* State)
	{
		if (!State) return;
		for (APlayerState* Player : State->PlayerArray)
		{
			auto* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
			if (!Pawn) continue;
			TInlineComponentArray<UWidgetComponent*> Components(Pawn);
			for (auto* Component : Components)
			{
				if (Component->GetFName() != TEXT("NameTagWidget")) continue;
				Component->InitWidget();
				auto* NameTag = Component->GetUserWidgetObject();
				auto* Text = NameTag ? Cast<UTextBlock>(NameTag->GetWidgetFromName(TEXT("Text_PlayerName"))) : nullptr;
				// 각 로비 폰의 복제된 PlayerState 이름을 기존 이름표에 표시한다.
				const FText Name = FText::FromString(Player->GetPlayerName());
				if (Text && !Text->GetText().EqualTo(Name)) Text->SetText(Name);
			}
		}
	}

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
	if (StartMatchButtonModule) StartMatchButton = FindModuleButton(StartMatchButtonModule, TEXT("Button_Action"));
	FriendsButton = FindModuleButton(FriendsButtonModule, TEXT("Button_Action"));
	LeavePartyButton = FindModuleButton(
		Cast<UUserWidget>(GetWidgetFromName(TEXT("WBP_Lobby_LeavePartyButton"))), TEXT("Button_Social"));
	if (LeavePartyButton)
		LeavePartyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeavePartyClicked);
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
	if (LeavePartyButton)
		LeavePartyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLeavePartyClicked);
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
	RefreshLobbyNameTags(State);
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
	const bool bCanLeave = IsDesignTime() || (bInLobby && Sessions->IsAutomaticInviteHubEnabled());
	if (LeavePartyButton) LeavePartyButton->SetIsEnabled(bCanLeave);

	// 파티 나가기는 나갈 파티가 있을 때만 보인다. 로비에 나 혼자면 숨긴다.
	// 친구 창이 열려 있는 동안에도 숨긴다. 이 버튼이 창 위에 겹쳐 그려지는 것을 막는다.
	if (auto* LeavePartyModule = GetWidgetFromName(TEXT("WBP_Lobby_LeavePartyButton")))
	{
		const bool bInParty = State && State->PlayerArray.Num() > 1;
		const bool bFriendsOpen = FriendsPanel && FriendsPanel->IsOpen();
		const bool bShowLeave = IsDesignTime() || (bInParty && !bFriendsOpen);
		LeavePartyModule->SetVisibility(bShowLeave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// Keep the designer's widgets, but retire their runtime Ready action.
	if (ReadyButtonModule) ReadyButtonModule->SetVisibility(ESlateVisibility::Collapsed);
	if (ReadyButton) ReadyButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UHBLobbyWidget::HandleFriendsClicked()
{
	if (FriendsPanel)
	{
		FriendsPanel->Toggle();
		// 창이 열리고 닫히는 즉시 파티 나가기 버튼의 표시도 맞춘다.
		RefreshLobbyActions();
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

void UHBLobbyWidget::HandleLeavePartyClicked()
{
	RefreshLobbyActions();
	if (!LeavePartyButton || !LeavePartyButton->GetIsEnabled()) return;
	auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
	if (Sessions && !Sessions->RequestReturnToOwnHub())
		UE_LOG(LogTemp, Warning, TEXT("LobbyWidget | 파티 나가기 요청을 처리하지 못했습니다."));
	RefreshLobbyActions();
}
#pragma endregion
