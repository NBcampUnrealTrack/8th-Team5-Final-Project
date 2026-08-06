// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameState.h"

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
