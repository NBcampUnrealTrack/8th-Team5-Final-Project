// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#include "HBLobbyPlayerController.h"
#include "PlayerState/HBLobbyPlayerState.h"
#include "HBLobbyGameState.h"

AHBLobbyGameMode::AHBLobbyGameMode()
{
	PlayerControllerClass = AHBLobbyPlayerController::StaticClass();
	PlayerStateClass = AHBLobbyPlayerState::StaticClass();
	GameStateClass = AHBLobbyGameState::StaticClass();
	
	DefaultPawnClass = nullptr;
}

void AHBLobbyGameMode::EvaluateCanStartMatch()
{
	AHBLobbyGameState* LobbyGameState = GetGameState<AHBLobbyGameState>();
	
	if (!IsValid(LobbyGameState))
	{
		return;
	}
	
	const int32 PlayerCount = LobbyGameState->PlayerArray.Num();
	
	if (PlayerCount < RequiredReadyPlayerCount)
	{
		LobbyGameState->SetCanStartMatch(false);
		return;
	}
	
	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		AHBLobbyPlayerState* LobbyPlayerState = Cast<AHBLobbyPlayerState>(PlayerState);
		
		if (!IsValid(LobbyPlayerState) || !LobbyPlayerState->IsReady())
		{
			LobbyGameState->SetCanStartMatch(false);
			return;
		}
	}
	
	LobbyGameState->SetCanStartMatch(true);
}

void AHBLobbyGameMode::TryStartGame()
{
	if (!HasAuthority())
	{
		return;
	}

	EvaluateCanStartMatch();

	AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		!LobbyGameState->CanStartMatch())
	{
		return;
	}

	if (GameplayMap.IsNull())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Lobby | GameplayMap is not set"));

		return;
	}

	const FString MapPath =
		GameplayMap.ToSoftObjectPath().GetLongPackageName();

	GetWorld()->ServerTravel(MapPath);
}
