// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

AHBLobbyPlayerState::AHBLobbyPlayerState()
{
	bIsReady = false;
}

void AHBLobbyPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (bIsReady == bNewReady)
	{
		return;
	}
	
	bIsReady = bNewReady;
	
	UE_LOG(
	LogTemp,
	Log,
	TEXT("Lobby | Player: %s | Ready: %s"),
	*GetPlayerName(),
	bIsReady ? TEXT("True") : TEXT("False")
	);
	
	OnReadyStateChanged.Broadcast(bIsReady);
}

void AHBLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AHBLobbyPlayerState, bIsReady);
}

void AHBLobbyPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast(bIsReady);
}
