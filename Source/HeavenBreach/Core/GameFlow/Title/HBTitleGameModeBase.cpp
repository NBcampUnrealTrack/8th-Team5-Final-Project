// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Title/HBTitleGameModeBase.h"
#include "Core/GameFlow/Title/PlayerController/HBTitlePlayerController.h"

AHBTitleGameModeBase::AHBTitleGameModeBase()
{
	PlayerControllerClass = AHBTitlePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
