// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Title/PlayerController/HBTitlePlayerController.h"

#include "UI/Title/HBGameStartMenuWidget.h"
#include "Kismet/KismetSystemLibrary.h"

void AHBTitlePlayerController::ConnectToLobby()
{
	if (!IsLocalController())
	{
		return;
	}
	
	if (LobbyServerAddress.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Title | LobbyServerAddress is empty"));
		return;
	}
	
	UE_LOG(LogTemp, Log,
	TEXT("Title | Connect To Lobby: %s"),
	*LobbyServerAddress);
	
	ClientTravel(
		LobbyServerAddress,
		ETravelType::TRAVEL_Absolute
		);
}

void AHBTitlePlayerController::ExitGame()
{
	if (!IsLocalController())
	{
		return;
	}
	
	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false
		);
}

void AHBTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}
	
	if (!GameStartMenuClass)
	{
		return;
	}
	
	GameStartMenu = CreateWidget<UHBGameStartMenuWidget>(
		this,
		GameStartMenuClass
		);
	if (!IsValid(GameStartMenu))
	{
		return;
	}
	
	GameStartMenu->AddToViewport();
	
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}
