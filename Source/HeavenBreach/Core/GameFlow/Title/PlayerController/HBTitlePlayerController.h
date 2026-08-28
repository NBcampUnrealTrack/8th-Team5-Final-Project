// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HBTitlePlayerController.generated.h"

class UHBGameStartMenuWidget;
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBTitlePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	void ConnectToLobby();
	void ExitGame();
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Title|UI")
	TSubclassOf<UHBGameStartMenuWidget> GameStartMenuClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Title|Travel")
	FName LobbyLevelName = TEXT("/Game/Levels/Lobby/L_Lobby");
	
private:
	UPROPERTY()
	TObjectPtr<UHBGameStartMenuWidget> GameStartMenu;
};
