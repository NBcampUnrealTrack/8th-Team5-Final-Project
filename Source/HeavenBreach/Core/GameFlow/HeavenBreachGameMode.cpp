// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameMode.h"

#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include  "Core/Online/Session/HBGameSession.h"

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
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Died | ActivePlayers: %d"), 
		ActivePlayers.Num()
		);
	
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
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Extracted | ActivePlayers: %d"),
		ActivePlayers.Num()
		);
	
	EvaluateMatchEnd();
}

void AHeavenBreachGameMode::TravelPlayersToPostMatchMap()
{
	if (!HasAuthority())
	{
		return;
	}

	const FString PostMatchMap =
		TEXT("/Game/Levels/test_jh/TestMap");

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();

		if (!IsValid(PlayerController))
		{
			continue;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Travel Player To PostMatch Map | %s"),
			*PostMatchMap
		);

		PlayerController->ClientTravel(PostMatchMap,TRAVEL_Absolute);
	}
}

void AHeavenBreachGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (!HasAuthority())
	{
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HeavenBreach | Player Connected | ConnectedPlayers: %d / %d"),
		GetConnectedPlayerCount(),
		ExpectedPlayerCount
		);
	
	TryStartExtractionMatch();
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
}

bool AHeavenBreachGameMode::ReadyToStartMatch_Implementation()
{
	return false;
}

bool AHeavenBreachGameMode::ReadyToEndMatch_Implementation()
{
	// 매치 종료 조건 추가 예정 

	return false;
}

void AHeavenBreachGameMode::TryStartExtractionMatch()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (GetMatchState() != MatchState::WaitingToStart)
	{
		return;
	}
	
	if (ExpectedPlayerCount <= 0)
	{
		return;
	}
	
	if (GetConnectedPlayerCount() < ExpectedPlayerCount)
	{
		return;
	}
	
	StartMatch();
}

void AHeavenBreachGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();
}

void AHeavenBreachGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	InitializeActivePlayers();
	StartMatchTimer();
}

void AHeavenBreachGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	
	StopMatchTimer();
	ActivePlayers.Reset();
	
	TravelPlayersToPostMatchMap();
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
