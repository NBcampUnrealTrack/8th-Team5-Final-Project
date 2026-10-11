// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

bool AHBLobbyGameState::HasValidStartRoster() const
{
	if (!IsValid(HostPlayerState) || !PlayerArray.Contains(HostPlayerState)
		|| PlayerArray.Num() > 4) return false;
	for (APlayerState* Player : PlayerArray)
	{
		if (!IsValid(Cast<AHBLobbyPlayerState>(Player))) return false;
	}
	return true;
}

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
	DOREPLIFETIME(AHBLobbyGameState, HostPlayerState);
	DOREPLIFETIME(AHBLobbyGameState, RoomName);
}

void AHBLobbyGameState::OnRep_CanStartMatch()
{
	OnCanStartMatchChanged.Broadcast(bCanStartMatch);
}
