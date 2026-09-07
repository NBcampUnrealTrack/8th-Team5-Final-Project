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
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyHUDWidget> LobbyWidgetClass;
	
private:
	// Client-side diagnostics use replicated state; no extra server RPCs.
	struct FLobbyDebugPlayer
	{
		FString Name;
		bool bReady = false;
	};
	void RefreshLobbyDebug();
	void ShowLobbyDebugEvent(const FString& Message);
	FTimerHandle LobbyDebugTimer;
	TMap<uint32, FLobbyDebugPlayer> LobbyDebugPlayers;
	bool bLobbyDebugInitialized = false;

	UPROPERTY()
	TObjectPtr<UHBLobbyHUDWidget> LobbyWidget;
};
