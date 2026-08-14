// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"

#include "PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "HBLobbyGameMode.h"

void AHBLobbyPlayerController::SetReady(bool bNewReady)
{
	if (!IsLocalController())
	{
		return;
	}
	
	ServerSetReady(bNewReady);
}

void AHBLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController())
	{
		return;
	}

	ServerRequestStartGame();
}

void AHBLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
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
