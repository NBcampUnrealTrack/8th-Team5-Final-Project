// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#include "HBLobbyPlayerController.h"
#include "PlayerState/HBLobbyPlayerState.h"
#include "HBLobbyGameState.h"
#include "Core/Online/Session/HBGameSession.h"
#include "Misc/ConfigCacheIni.h"

namespace HBLobbyFlow
{
	const TCHAR* ConfigSection = TEXT("HeavenBreach.Lobby");
	const TCHAR* GameplayMapKey = TEXT("GameplayMap");
	const TCHAR* DefaultGameplayMap =
		TEXT("/Game/Levels/TierA/Maps/Floor_A.Floor_A");
}

AHBLobbyGameMode::AHBLobbyGameMode()
{
	PlayerControllerClass = AHBLobbyPlayerController::StaticClass();
	PlayerStateClass = AHBLobbyPlayerState::StaticClass();
	GameStateClass = AHBLobbyGameState::StaticClass();
	GameSessionClass = AHBGameSession::StaticClass();
	
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;

	// Keep the native default usable without editing the lobby map or its
	// Blueprint GameMode asset.
	GameplayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		HBLobbyFlow::DefaultGameplayMap
		));
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

void AHBLobbyGameMode::HandleReadyStateChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	// A confirmation applies to one exact ready roster.  If anyone toggles
	// ready, every player must approve the new roster again.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
}

void AHBLobbyGameMode::ConfirmStart(
	AHBLobbyPlayerState* RequestingPlayerState)
{
	if (!HasAuthority() || bTravelRequested || !IsValid(RequestingPlayerState))
	{
		return;
	}

	EvaluateCanStartMatch();

	const AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		!LobbyGameState->CanStartMatch() ||
		!LobbyGameState->PlayerArray.Contains(RequestingPlayerState))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Start confirmation rejected: ready roster is incomplete")
			);
		return;
	}

	RequestingPlayerState->SetStartConfirmed(true);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Lobby | Player confirmed match start: %s"),
		*RequestingPlayerState->GetPlayerName()
		);

	TryStartGame();
}

void AHBLobbyGameMode::TryStartGame()
{
	if (!HasAuthority() || bTravelRequested)
	{
		return;
	}

	EvaluateCanStartMatch();

	AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		!LobbyGameState->CanStartMatch() ||
		!AreAllPlayersReadyAndConfirmed())
	{
		return;
	}

	FString ConfiguredGameplayMap;
	const bool bHasConfiguredGameplayMap = GConfig != nullptr &&
		GConfig->GetString(
			HBLobbyFlow::ConfigSection,
			HBLobbyFlow::GameplayMapKey,
			ConfiguredGameplayMap,
			GEngineIni
			);

	// INI wins over the serialized Blueprint value so the VM destination can
	// be corrected without modifying the user-protected GameMode asset.
	const FString MapPath = bHasConfiguredGameplayMap
		? FSoftObjectPath(ConfiguredGameplayMap).GetLongPackageName()
		: GameplayMap.ToSoftObjectPath().GetLongPackageName();

	if (MapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Lobby | GameplayMap package path is invalid"));
		return;
	}

	// Only the VM's authoritative GameMode can reach this point.  ServerTravel
	// moves every connected client together while preserving the Steam session.
	bTravelRequested = true;
	UE_LOG(LogTemp, Log,
		TEXT("Lobby | All players confirmed | ServerTravel: %s"),
		*MapPath);
	GetWorld()->ServerTravel(MapPath);
}

void AHBLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	// A newly connected player changes the group that is approving the match.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
}

void AHBLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

	// Removing a player also changes the approved group; stale confirmations
	// must never make the smaller group start automatically.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
}

void AHBLobbyGameMode::ResetStartConfirmations()
{
	AHBLobbyGameState* LobbyGameState = GetGameState<AHBLobbyGameState>();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		if (AHBLobbyPlayerState* LobbyPlayerState =
			Cast<AHBLobbyPlayerState>(PlayerState))
		{
			LobbyPlayerState->SetStartConfirmed(false);
		}
	}
}

bool AHBLobbyGameMode::AreAllPlayersReadyAndConfirmed() const
{
	const AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		LobbyGameState->PlayerArray.Num() < RequiredReadyPlayerCount)
	{
		return false;
	}

	for (const APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		const AHBLobbyPlayerState* LobbyPlayerState =
			Cast<AHBLobbyPlayerState>(PlayerState);

		if (!IsValid(LobbyPlayerState) ||
			!LobbyPlayerState->IsReady() ||
			!LobbyPlayerState->IsStartConfirmed())
		{
			return false;
		}
	}

	return true;
}
