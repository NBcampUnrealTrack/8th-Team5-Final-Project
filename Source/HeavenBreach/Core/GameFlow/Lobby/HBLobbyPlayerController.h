// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HBLobbyPlayerController.generated.h"

class UHBLobbyHUDWidget;
class UHBLobbyWidget;
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void SetReady(bool bNewReady);
	
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();
	
protected:
	virtual void BeginPlay() override;
	
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyHUDWidget> LobbyWidgetClass;
	
private:
	UPROPERTY()
	TObjectPtr<UHBLobbyHUDWidget> LobbyWidget;
};
