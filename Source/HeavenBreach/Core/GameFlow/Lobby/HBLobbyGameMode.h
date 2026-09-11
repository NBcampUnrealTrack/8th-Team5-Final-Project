// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "HBLobbyGameMode.generated.h"

class UWorld;
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AHBLobbyGameMode();
	
	void EvaluateCanStartMatch();
	void HandleReadyStateChanged();
	void ConfirmStart(class AHBLobbyPlayerState* RequestingPlayerState);
	void TryStartGame();
	
protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 RequiredReadyPlayerCount = 2;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	TSoftObjectPtr<UWorld> GameplayMap;

private:
	void ResetStartConfirmations();
	bool AreAllPlayersReadyAndConfirmed() const;

	/** Prevents duplicate clicks in the final frame from issuing multiple server travels. */
	bool bTravelRequested = false;
};
