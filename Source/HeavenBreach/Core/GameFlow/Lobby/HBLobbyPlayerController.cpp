// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"

#include "PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"
#include "HBLobbyGameMode.h"
#include "HBLobbyGameState.h"

void AHBLobbyPlayerController::SetReady(bool bNewReady)
{
	if (!IsLocalController())
	{
		return;
	}

	// All machines are clients in the VM topology.  Only the authoritative
	// dedicated server is allowed to change the replicated ready state.
	ServerSetReady(bNewReady);
}

void AHBLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController())
	{
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

	// This is a per-player confirmation, not a host command.  The server RPC
	// identifies the sender from its owning controller and never trusts a
	// client-supplied player or Steam ID.
	ServerRequestStartGame();
	return true;
}

void AHBLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}

	LobbyWidget = CreateWidget<UHBLobbyHUDWidget>(
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

void AHBLobbyPlayerController::ServerRequestStartGame_Implementation()
{
	AHBLobbyGameMode* LobbyGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AHBLobbyGameMode>()
		: nullptr;

	if (!IsValid(LobbyGameMode))
	{
		return;
	}

	// The controller's own PlayerState is the server-side identity of the
	// caller, so a client cannot confirm on behalf of another player.
	LobbyGameMode->ConfirmStart(
		GetPlayerState<AHBLobbyPlayerState>()
		);
}

void AHBLobbyPlayerController::ServerSetReady_Implementation(bool bNewReady)
{
	AHBLobbyPlayerState* LobbyPlayerState = GetPlayerState<AHBLobbyPlayerState>();
	
	if (!IsValid(LobbyPlayerState))
	{
		return;
	}
	
	const bool bReadyStateChanged =
		LobbyPlayerState->IsReady() != bNewReady;

	LobbyPlayerState->SetReady(bNewReady);
	
	AHBLobbyGameMode* LobbyGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AHBLobbyGameMode>()
		: nullptr;

	if (IsValid(LobbyGameMode) && bReadyStateChanged)
	{
		// Any ready-list change invalidates votes cast for the previous group.
		LobbyGameMode->HandleReadyStateChanged();
	}
}
