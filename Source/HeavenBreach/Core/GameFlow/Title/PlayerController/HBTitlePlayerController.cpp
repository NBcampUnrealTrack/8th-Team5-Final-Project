// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Title/PlayerController/HBTitlePlayerController.h"

#include "UI/Title/HBGameStartMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void AHBTitlePlayerController::ConnectToLobby()
{
	if (!IsLocalController())
	{
		return;
	}
	
	if (LobbyLevelName.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Title | LobbyLevelName is empty"));
		return;
	}
	
	UE_LOG(LogTemp, Log,
		TEXT("Title | Open Lobby Level: %s"),
		*LobbyLevelName.ToString());
	
	UGameplayStatics::OpenLevel(this, LobbyLevelName);
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
