// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameMode.h"

#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include  "Core/Online/Session/HBGameSession.h"

namespace HBMatchFlow
{
	const TCHAR* PostMatchMap = TEXT("/Game/Levels/ExtractionResult");
}

AHeavenBreachGameMode::AHeavenBreachGameMode()
{
	bDelayedStart = true;

	GameStateClass = AHeavenBreachGameState::StaticClass();
	PlayerControllerClass = AHBPlayerController::StaticClass();
	PlayerStateClass = AHB_PlayerState::StaticClass();
	DefaultPawnClass = AHBPlayerCharacter::StaticClass();
	
	GameSessionClass = AHBGameSession::StaticClass();
}

void AHeavenBreachGameMode::StartExtractionMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingToStart)
	{
		return;
	}

	StartMatch();
}

void AHeavenBreachGameMode::FinishExtractionMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsMatchInProgress())
	{
		return;
	}

	EndMatch();
}

int32 AHeavenBreachGameMode::GetConnectedPlayerCount() 
{
	return GetNumPlayers();
}

void AHeavenBreachGameMode::NotifyPlayerDied(APlayerState* PlayerState)
{
	if (!HasAuthority() || !IsMatchInProgress() || !IsValid(PlayerState))
	{
		return;
	}
	
	const int32 RemovedCount = ActivePlayers.Remove(PlayerState);
	if (RemovedCount == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Player Died but was not active | Player: %s | ActivePlayers: %d"),
			*PlayerState->GetPlayerName(),
			ActivePlayers.Num()
			);

		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Died | ActivePlayers: %d"), 
		ActivePlayers.Num()
		);

	TravelPlayerToPostMatchMap(PlayerState, EHBPostMatchReason::Died);
	EvaluateMatchEnd();
}

void AHeavenBreachGameMode::NotifyPlayerExtracted(APlayerState* PlayerState)
{
	if (!HasAuthority() || !IsMatchInProgress() || !IsValid(PlayerState))
	{
		return;
	}
	
	const int32 RemovedCount = ActivePlayers.Remove(PlayerState);
	if (RemovedCount == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Player Extracted but was not active | Player: %s | ActivePlayers: %d"),
			*PlayerState->GetPlayerName(),
			ActivePlayers.Num()
			);

		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Extracted | ActivePlayers: %d"),
		ActivePlayers.Num()
		);

	TravelPlayerToPostMatchMap(PlayerState, EHBPostMatchReason::Extracted);
	EvaluateMatchEnd();
}

void AHeavenBreachGameMode::TravelPlayersToPostMatchMap(EHBPostMatchReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();

		if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
		{
			continue;
		}

		TravelPlayerToPostMatchMap(PlayerController->PlayerState, Reason);
	}
}

void AHeavenBreachGameMode::TravelPlayerToPostMatchMap(APlayerState* PlayerState, EHBPostMatchReason Reason)
{
	if (!HasAuthority() || !IsValid(PlayerState) || PostMatchTravelRequestedPlayers.Contains(PlayerState))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsValid(PlayerController) || PlayerController->PlayerState != PlayerState)
		{
			continue;
		}

		PostMatchTravelRequestedPlayers.Add(PlayerState);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Travel Player To PostMatch Map | Player: %s | Map: %s | Reason: %d"),
			*PlayerState->GetPlayerName(),
			HBMatchFlow::PostMatchMap,
			static_cast<int32>(Reason)
			);

		// PlayerState/GameState don't survive this travel, so hand the reason to the
		// client's GameInstance subsystem before it disconnects into the results map.
		if (AHBPlayerController* HBPlayerController = Cast<AHBPlayerController>(PlayerController))
		{
			HBPlayerController->ClientSetPostMatchReason(Reason);
		}

		PlayerController->ClientTravel(HBMatchFlow::PostMatchMap, TRAVEL_Absolute);
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("HeavenBreachGM | PostMatch travel failed: PlayerController not found | Player: %s"),
		*PlayerState->GetPlayerName()
		);
}

void AHeavenBreachGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Both PostLogin and seamless player migration reach this hook. Let the
	// engine restart players joining an active match before updating our roster.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (!HasAuthority())
	{
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HeavenBreach | Player initialized | ConnectedPlayers: %d / %d"),
		GetConnectedPlayerCount(),
		MinimumPlayersToStart
		);

	if (IsMatchInProgress())
	{
		if (IsValid(NewPlayer) && IsValid(NewPlayer->PlayerState)
			&& !ActivePlayers.Contains(NewPlayer->PlayerState))
		{
			ActivePlayers.Add(NewPlayer->PlayerState);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("HeavenBreachGM | Late player added to active match | Player: %s | ActivePlayers: %d"),
				*NewPlayer->PlayerState->GetPlayerName(),
				ActivePlayers.Num()
				);
		}

		return;
	}
}

void AHeavenBreachGameMode::Logout(AController* Exiting)
{
	APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
	
	if (IsMatchInProgress() && IsValid(ExitingPlayerState))
	{
		const int32 RemovedCount =
		   ActivePlayers.Remove(ExitingPlayerState);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("HeavenBreachGM | Player Logout | Removed: %d | ActivePlayers: %d"),
			RemovedCount,
			ActivePlayers.Num()
		);
		//ActivePlayers.Remove(ExitingPlayerState);
	}
	
	Super::Logout(Exiting);
	
	EvaluateMatchEnd();

	if (GetMatchState() == MatchState::WaitingPostMatch)
	{
		ScheduleMatchMapReload();
	}
}

bool AHeavenBreachGameMode::ReadyToStartMatch_Implementation()
{
	// GetNumPlayers() includes clients still travelling with their lobby state.
	// Wait for migration to finish before taking the initial active-player roster.
	// Keep this a predicate: AGameMode starts the match after the current state
	// transition completes, or when an initialized player is ready to start.
	return HasAuthority()
		&& GetMatchState() == MatchState::WaitingToStart
		&& bAutoStartMatch
		&& NumTravellingPlayers == 0
		&& NumPlayers >= FMath::Max(1, MinimumPlayersToStart);
}

bool AHeavenBreachGameMode::ReadyToEndMatch_Implementation()
{
	// 매치 종료 조건 추가 예정

	return false;
}

void AHeavenBreachGameMode::HandleMatchHasStarted()
{
	UE_LOG(LogTemp, Log,
		TEXT("HeavenBreachGM | Starting match | InitializedPlayers: %d | Required: %d"),
		NumPlayers, FMath::Max(1, MinimumPlayersToStart));

	Super::HandleMatchHasStarted();
	
	InitializeActivePlayers();
	StartMatchTimer();
}

void AHeavenBreachGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	
	StopMatchTimer();
	ActivePlayers.Reset();

	TravelPlayersToPostMatchMap(EHBPostMatchReason::MatchEnded);
	ScheduleMatchMapReload();
}

void AHeavenBreachGameMode::HandleLeavingMap()
{
	Super::HandleLeavingMap();
}

void AHeavenBreachGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
}

void AHeavenBreachGameMode::InitializeActivePlayers()
{
	ActivePlayers.Reset();
	PostMatchTravelRequestedPlayers.Reset();
	
	AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>();
	if (!HBGameState)
	{
		return;
	}
	
	for (APlayerState* PlayerState : HBGameState->PlayerArray)
	{
		if (IsValid(PlayerState))
		{
			ActivePlayers.Add(PlayerState);
		}
	}
	
	UE_LOG(
	LogTemp,
	Log,
	TEXT("HeavenBreachGM | ActivePlayers Initialized : %d"),
	ActivePlayers.Num()
);
}

void AHeavenBreachGameMode::EvaluateMatchEnd()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsMatchInProgress())
	{
		return;
	}
	
	if (!ActivePlayers.IsEmpty())
	{
		return;
	}
	
	FinishExtractionMatch();
}

void AHeavenBreachGameMode::ScheduleMatchMapReload()
{
	if (!HasAuthority() || GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingPostMatch || bMatchMapReloadScheduled)
	{
		return;
	}

	bMatchMapReloadScheduled = true;

	GetWorldTimerManager().SetTimer(
		MatchMapReloadTimerHandle,
		this,
		&AHeavenBreachGameMode::TryReloadMatchMap,
		MatchMapReloadRetrySeconds,
		false
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match map reload scheduled | RetryDelay: %.2f"),
		MatchMapReloadRetrySeconds
	);
}

void AHeavenBreachGameMode::TryReloadMatchMap()
{
	bMatchMapReloadScheduled = false;

	if (!HasAuthority() || GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingPostMatch)
	{
		return;
	}

	const int32 ConnectedPlayerCount = GetConnectedPlayerCount();
	if (ConnectedPlayerCount > 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("HeavenBreachGM | Waiting to reload match map | ConnectedPlayers: %d"),
			ConnectedPlayerCount
		);

		ScheduleMatchMapReload();
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HeavenBreachGM | Reloading match map for next match")
	);

	RestartGame();
}

void AHeavenBreachGameMode::StartMatchTimer()
{
	if (!HasAuthority())
	{
		return;
	}
	
	AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>();
	if (!HBGameState)
	{
		return;
	}
	
	const double EndServerTime = HBGameState->GetServerWorldTimeSeconds() + static_cast<double>(MatchDurationSeconds);
	HBGameState->SetMatchEndServerTime(EndServerTime);
	
	GetWorldTimerManager().SetTimer(
		MatchTimerHandle,
		this,
		&AHeavenBreachGameMode::HandleMatchTimeExpired,
		MatchDurationSeconds,
		false
		);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match Timer Started | Duration : %.2f"), MatchDurationSeconds
		);
}

void AHeavenBreachGameMode::StopMatchTimer()
{
	if (!HasAuthority())
	{
		return;
	}
	
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	
	if (AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>())
	{
		HBGameState->SetMatchEndServerTime(0.0f);
	}
}

void AHeavenBreachGameMode::HandleMatchTimeExpired()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsMatchInProgress())
	{
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match Time Expired")
		);
	
	FinishExtractionMatch();
}
