// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Title/HBGameStartMenuWidget.h"
#include "Components/Button.h"
#include "Core/GameFlow/Title/PlayerController/HBTitlePlayerController.h"

void UHBGameStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (StartButton)
	{
		StartButton->OnClicked.AddUniqueDynamic(
			this,
			&UHBGameStartMenuWidget::HandleStartClicked
			);
	}
	
	if (ExitButton)
	{
		ExitButton->OnClicked.AddUniqueDynamic(
			this,
			&UHBGameStartMenuWidget::HandleExitClicked
			);
	}
}

void UHBGameStartMenuWidget::HandleStartClicked()
{
	AHBTitlePlayerController* TitlePC = GetOwningPlayer<AHBTitlePlayerController>();
	
	if (!IsValid(TitlePC))
	{
		return;
	}
	
	TitlePC->ConnectToLobby();
}

void UHBGameStartMenuWidget::HandleExitClicked()
{
	AHBTitlePlayerController* TitlePC =
		GetOwningPlayer<AHBTitlePlayerController>();

	if (!IsValid(TitlePC))
	{
		return;
	}

	TitlePC->ExitGame();
}
