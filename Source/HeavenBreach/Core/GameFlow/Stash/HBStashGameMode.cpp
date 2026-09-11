// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Stash/HBStashGameMode.h"

#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"

AHBStashGameMode::AHBStashGameMode()
{
	PlayerControllerClass = AHBPlayerController::StaticClass();
	DefaultPawnClass = AHBPlayerCharacter::StaticClass();
}
