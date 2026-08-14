// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameState.h"

#include "Net/UnrealNetwork.h"

void AHBLobbyGameState::SetCanStartMatch(bool bNewCanStartMatch)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (bCanStartMatch == bNewCanStartMatch)
	{
		return;
	}
	
	bCanStartMatch = bNewCanStartMatch;
	
	OnCanStartMatchChanged.Broadcast(bCanStartMatch);
}

void AHBLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AHBLobbyGameState, bCanStartMatch);
}

void AHBLobbyGameState::OnRep_CanStartMatch()
{
	OnCanStartMatchChanged.Broadcast(bCanStartMatch);
}
