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

	/** Compatibility entry point used by the existing UI; the dedicated server decides whether the vote is accepted. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Match")
	bool RequestStartDedicatedMatch();

	/** JobId must match a UHBJobData::JobId (e.g. "Warrior", "Mage", "Archer"). The server re-validates it. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Job")
	void SelectJob(FName JobId);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	UFUNCTION(Server, Reliable)
	void ServerSelectJob(FName JobId);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyHUDWidget> LobbyWidgetClass;
	
private:
	UPROPERTY()
	TObjectPtr<UHBLobbyHUDWidget> LobbyWidget;
};
