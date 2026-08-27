// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "HBLobbyPlayerController.generated.h"

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

	/** Requests the Steam lobby owner's dedicated-server match flow. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Match")
	bool RequestStartDedicatedMatch();
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyWidget> LobbyWidgetClass;

	/** Temporary lobby size used until host/client roles have their own flow. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Temporary")
	int32 TemporaryLobbyMaxPlayers = 10;
	
private:
	UFUNCTION()
	void HandleLobbyOperationStateChanged(
		EHBLobbyOperationState State,
		const FString& ErrorMessage
		);

	bool GetTemporaryLobbyRole(FString& OutRole) const;
	void UpdateTemporaryStartPermission(EHBLobbyOperationState State);

	UPROPERTY()
	TObjectPtr<UHBLobbyWidget> LobbyWidget;
};
