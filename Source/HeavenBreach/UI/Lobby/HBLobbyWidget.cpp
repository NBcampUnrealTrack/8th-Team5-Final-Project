// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Lobby/HBLobbyWidget.h"

#include "Components/Button.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"

void UHBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddUniqueDynamic(
			this,
			&UHBLobbyWidget::HandleReadyClicked
			);
	}
	
	if (StartMatchButton)
	{
		StartMatchButton->OnClicked.AddUniqueDynamic(
			this,
			&UHBLobbyWidget::HandleStartMatchClicked
			);
		
		// All Ready 전에는 비활성화 
		StartMatchButton->SetIsEnabled(false);
	}
	
	LobbyGameState = GetWorld()->GetGameState<AHBLobbyGameState>();
	if (IsValid(LobbyGameState))
	{
		LobbyGameState->OnCanStartMatchChanged.AddUniqueDynamic(
			this,
			&UHBLobbyWidget::HandleCanStartMatchChanged
			);
		
		HandleCanStartMatchChanged(LobbyGameState->CanStartMatch());
	}
	
}

void UHBLobbyWidget::NativeDestruct()
{
	if (IsValid(LobbyGameState))
	{
		LobbyGameState->OnCanStartMatchChanged.RemoveDynamic(
			this,
			&UHBLobbyWidget::HandleCanStartMatchChanged);
	}

	Super::NativeDestruct();
}

void UHBLobbyWidget::HandleReadyClicked()
{
	AHBLobbyPlayerController* LobbyPC = GetOwningPlayer<AHBLobbyPlayerController>();
	AHBLobbyPlayerState* LobbyPS = GetOwningPlayerState<AHBLobbyPlayerState>();
	
	if (!IsValid(LobbyPC) || !IsValid(LobbyPS))
	{
		return;
	}
	
	LobbyPC->SetReady(!LobbyPS->IsReady());
}

void UHBLobbyWidget::HandleStartMatchClicked()
{
	AHBLobbyPlayerController* LobbyPC =
	GetOwningPlayer<AHBLobbyPlayerController>();

	if (!IsValid(LobbyPC))
	{
		return;
	}

	LobbyPC->RequestStartGame();
}

void UHBLobbyWidget::HandleCanStartMatchChanged(bool bCanStartMatch)
{
	if (StartMatchButton)
	{
		StartMatchButton->SetIsEnabled(bCanStartMatch);
	}
}