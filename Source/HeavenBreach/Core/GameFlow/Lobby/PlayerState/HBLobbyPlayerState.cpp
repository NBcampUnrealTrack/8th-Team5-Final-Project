// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

AHBLobbyPlayerState::AHBLobbyPlayerState()
{
	bIsReady = false;
	bStartConfirmed = false;
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
	ForceNetUpdate();
}

void AHBLobbyPlayerState::SetStartConfirmed(bool bNewStartConfirmed)
{
	if (!HasAuthority() || bStartConfirmed == bNewStartConfirmed)
	{
		return;
	}

	// The dedicated server owns this flag; clients can only request their own
	// confirmation through their PlayerController RPC.
	bStartConfirmed = bNewStartConfirmed;
	OnStartConfirmationChanged.Broadcast(bStartConfirmed);
	ForceNetUpdate();
}

void AHBLobbyPlayerState::SetSelectedJobId(FName NewJobId)
{
	if (!HasAuthority() || SelectedJobId == NewJobId)
	{
		return;
	}

	SelectedJobId = NewJobId;

	UE_LOG(
	LogTemp,
	Log,
	TEXT("Lobby | Player: %s | SelectedJob: %s"),
	*GetPlayerName(),
	*SelectedJobId.ToString()
	);

	OnSelectedJobChanged.Broadcast(SelectedJobId);
	ForceNetUpdate();
}

void AHBLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHBLobbyPlayerState, bIsReady);
	DOREPLIFETIME(AHBLobbyPlayerState, bStartConfirmed);
	DOREPLIFETIME(AHBLobbyPlayerState, SelectedJobId);
}

void AHBLobbyPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast(bIsReady);
}

void AHBLobbyPlayerState::OnRep_StartConfirmed()
{
	OnStartConfirmationChanged.Broadcast(bStartConfirmed);
}

void AHBLobbyPlayerState::OnRep_SelectedJobId()
{
	OnSelectedJobChanged.Broadcast(SelectedJobId);
}
