// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"

#include "PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "HBLobbyGameMode.h"
#include "HBLobbyGameState.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HBLobbyTemporary
{
	const TCHAR* RoleCommandLineKey = TEXT("HBTempLobbyRole=");
	const TCHAR* HostRole = TEXT("Host");
	const TCHAR* ClientRole = TEXT("Client");
}

void AHBLobbyPlayerController::SetReady(bool bNewReady)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!bNewReady)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Temporary ready flow ignores unready requests")
			);

		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UHBOnlineSessionSubsystem* OnlineSessionSubsystem = GameInstance != nullptr
		? GameInstance->GetSubsystem<UHBOnlineSessionSubsystem>()
		: nullptr;

	if (OnlineSessionSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | Ready failed: OnlineSessionSubsystem unavailable")
			);

		return;
	}

	FString TemporaryRole;
	if (!GetTemporaryLobbyRole(TemporaryRole))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | Ready failed: Use -HBTempLobbyRole=Host or Client")
			);

		return;
	}

	if (TemporaryRole.Equals(
		HBLobbyTemporary::HostRole,
		ESearchCase::IgnoreCase))
	{
		OnlineSessionSubsystem->RequestCreateLobby(TemporaryLobbyMaxPlayers);
		return;
	}

	OnlineSessionSubsystem->RequestFindAndJoinLobby();
}

void AHBLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController())
	{
		return;
	}

	FString TemporaryRole;
	if (!GetTemporaryLobbyRole(TemporaryRole) ||
		!TemporaryRole.Equals(
			HBLobbyTemporary::HostRole,
			ESearchCase::IgnoreCase))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Temporary role is not Host")
			);

		return;
	}

	RequestStartDedicatedMatch();
}

bool AHBLobbyPlayerController::RequestStartDedicatedMatch()
{
	if (!IsLocalController())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Controller is not local")
			);

		return false;
	}

	FString TemporaryRole;
	if (!GetTemporaryLobbyRole(TemporaryRole) ||
		!TemporaryRole.Equals(
			HBLobbyTemporary::HostRole,
			ESearchCase::IgnoreCase))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Temporary role is not Host")
			);

		return false;
	}

	const AHBLobbyGameState* LobbyGameState =
		GetWorld() != nullptr
			? GetWorld()->GetGameState<AHBLobbyGameState>()
			: nullptr;

	if (!IsValid(LobbyGameState) || !LobbyGameState->CanStartMatch())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Ready conditions are not satisfied")
			);

		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | Match start failed: GameInstance unavailable")
			);

		return false;
	}

	UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
		GameInstance->GetSubsystem<UHBOnlineSessionSubsystem>();

	if (OnlineSessionSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | Match start failed: OnlineSessionSubsystem unavailable")
			);

		return false;
	}

	return OnlineSessionSubsystem->RequestStartMatch();
}

void AHBLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
			GameInstance->GetSubsystem<UHBOnlineSessionSubsystem>())
		{
			OnlineSessionSubsystem->OnLobbyOperationStateChanged.AddUniqueDynamic(
				this,
				&AHBLobbyPlayerController::HandleLobbyOperationStateChanged
				);

			HandleLobbyOperationStateChanged(
				OnlineSessionSubsystem->GetLobbyOperationState(),
				OnlineSessionSubsystem->GetLastLobbyOperationError()
				);
		}
	}
	
	LobbyWidget = CreateWidget<UHBLobbyWidget>(
		this,
		LobbyWidgetClass
		);
	
	if (!IsValid(LobbyWidget))
	{
		return;
	}
	
	LobbyWidget->AddToViewport();
	
	FInputModeUIOnly InputMode;
	//InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AHBLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
			GameInstance->GetSubsystem<UHBOnlineSessionSubsystem>())
		{
			OnlineSessionSubsystem->OnLobbyOperationStateChanged.RemoveDynamic(
				this,
				&AHBLobbyPlayerController::HandleLobbyOperationStateChanged
				);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AHBLobbyPlayerController::HandleLobbyOperationStateChanged(
	EHBLobbyOperationState State,
	const FString& ErrorMessage)
{
	UpdateTemporaryStartPermission(State);

	if (State == EHBLobbyOperationState::Failed)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | Temporary session flow failed: %s"),
			*ErrorMessage
			);
	}
}

bool AHBLobbyPlayerController::GetTemporaryLobbyRole(FString& OutRole) const
{
	OutRole.Reset();

	if (!FParse::Value(
		FCommandLine::Get(),
		HBLobbyTemporary::RoleCommandLineKey,
		OutRole))
	{
		return false;
	}

	OutRole.TrimStartAndEndInline();

	return OutRole.Equals(
			HBLobbyTemporary::HostRole,
			ESearchCase::IgnoreCase) ||
		OutRole.Equals(
			HBLobbyTemporary::ClientRole,
			ESearchCase::IgnoreCase);
}

void AHBLobbyPlayerController::UpdateTemporaryStartPermission(
	EHBLobbyOperationState State)
{
	AHBLobbyGameState* LobbyGameState = GetWorld() != nullptr
		? GetWorld()->GetGameState<AHBLobbyGameState>()
		: nullptr;

	if (!IsValid(LobbyGameState))
	{
		return;
	}

	FString TemporaryRole;
	const bool bCanStartAsTemporaryHost =
		GetTemporaryLobbyRole(TemporaryRole) &&
		TemporaryRole.Equals(
			HBLobbyTemporary::HostRole,
			ESearchCase::IgnoreCase) &&
		State == EHBLobbyOperationState::InLobby;

	LobbyGameState->SetCanStartMatch(bCanStartAsTemporaryHost);
}

void AHBLobbyPlayerController::ServerRequestStartGame_Implementation()
{
	AHBLobbyGameMode* LobbyGameMode =
	GetWorld()->GetAuthGameMode<AHBLobbyGameMode>();

	if (!IsValid(LobbyGameMode))
	{
		return;
	}

	LobbyGameMode->TryStartGame();
}

void AHBLobbyPlayerController::ServerSetReady_Implementation(bool bNewReady)
{
	AHBLobbyPlayerState* LobbyPlayerState = GetPlayerState<AHBLobbyPlayerState>();
	
	if (!IsValid(LobbyPlayerState))
	{
		return;
	}
	
	LobbyPlayerState->SetReady(bNewReady);
	
	AHBLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<AHBLobbyGameMode>();
	if (IsValid(LobbyGameMode))
	{
		LobbyGameMode->EvaluateCanStartMatch();
	}
}
