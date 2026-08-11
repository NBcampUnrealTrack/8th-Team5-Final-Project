// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameState.h"

#include "Net/UnrealNetwork.h"

void AHeavenBreachGameState::SetMatchEndServerTime(double InEndServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchEndServerTime = InEndServerTime;
}

double AHeavenBreachGameState::GetRemainingMatchTime()
{
	if (MatchEndServerTime <= 0.0)
	{
		return 0.0;
	}

	return FMath::Max(0.0f, MatchEndServerTime - GetServerWorldTimeSeconds());
}

void AHeavenBreachGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeavenBreachGameState, MatchEndServerTime);
}

void AHeavenBreachGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	const FName CurrentMatchState = GetMatchState();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM|MatchState : %s"),
		*CurrentMatchState.ToString()
	);

	OnMatchStateChanged.Broadcast(CurrentMatchState);
}

void AHeavenBreachGameState::OnRep_MatchEndServerTime()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGS | MatchEndServerTime : %.2f"),
		MatchEndServerTime
	);
}
