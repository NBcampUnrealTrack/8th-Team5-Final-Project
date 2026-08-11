// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameMode.h"

#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"

AHeavenBreachGameMode::AHeavenBreachGameMode()
{
	bDelayedStart = true;

	GameStateClass = AHeavenBreachGameState::StaticClass();
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
}

void AHeavenBreachGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}

void AHeavenBreachGameMode::HandleLeavingMap()
{
	Super::HandleLeavingMap();
}

void AHeavenBreachGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
}
