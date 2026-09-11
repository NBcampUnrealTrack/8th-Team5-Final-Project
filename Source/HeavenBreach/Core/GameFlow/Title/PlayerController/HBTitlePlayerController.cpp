// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Title/PlayerController/HBTitlePlayerController.h"

#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "UI/Title/HBGameStartMenuWidget.h"
#include "Kismet/KismetSystemLibrary.h"

void AHBTitlePlayerController::ConnectToLobby()
{
	if (!IsLocalController())
	{
		return;
	}
	
	UGameInstance* GameInstance = GetGameInstance();
	UHBOnlineSessionSubsystem* OnlineSessionSubsystem = GameInstance != nullptr
		? GameInstance->GetSubsystem<UHBOnlineSessionSubsystem>()
		: nullptr;

	if (OnlineSessionSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Title | Dedicated server search failed: OnlineSessionSubsystem unavailable")
			);
		return;
	}

	// The title button never opens a local lobby map.  Every player executes
	// this same Steam search/join flow and the resolved VM server connection
	// performs the travel after JoinSession succeeds.
	OnlineSessionSubsystem->RequestFindAndJoinLobby();
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
