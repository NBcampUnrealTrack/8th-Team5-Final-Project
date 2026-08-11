// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameMode.h"

#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"

AHeavenBreachGameMode::AHeavenBreachGameMode()
{
	bDelayedStart = true;

	GameStateClass = AHeavenBreachGameState::StaticClass();
	PlayerControllerClass = AHBPlayerController::StaticClass();
	PlayerStateClass = AHB_PlayerState::StaticClass();
	DefaultPawnClass = AHBPlayerCharacter::StaticClass();
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
}

void AHeavenBreachGameMode::NotifyPlayerExtracted(APlayerState* PlayerState)
{
}

void AHeavenBreachGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void AHeavenBreachGameMode::Logout(AController* Exiting)
{

	Super::Logout(Exiting);
}

bool AHeavenBreachGameMode::ReadyToStartMatch_Implementation()
{
	if (!bAutoStartMatch)
	{
		return false;
	}

	return GetConnectedPlayerCount() >= MinimumPlayersToStart;
}

bool AHeavenBreachGameMode::ReadyToEndMatch_Implementation()
{
	// 매치 종료 조건 추가 예정 

	return false;
}

void AHeavenBreachGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();
}

void AHeavenBreachGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	StartMatchTimer();
}

void AHeavenBreachGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	
	StopMatchTimer();
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
	InitializeActivePlayers().
}

void AHeavenBreachGameMode::EvaluateMatchEnd()
{
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
	
	if (!IsMatchInProgress)
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
