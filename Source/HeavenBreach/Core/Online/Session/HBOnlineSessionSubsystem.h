// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "HBOnlineSessionSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UHBOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void CreateLobby(int32 MaxPlayers);
	void FindLobbies();
	
private:
	void HandleCreateLobbyComplete(FName InSessionName, bool bWasSuccessful);
	
	void HandleFindLobbiesComplete(bool bWasSuccessful);
	
	IOnlineSessionPtr SessionInterface;
	
	TSharedPtr<FOnlineSessionSettings> LobbySettings;
	TSharedPtr<FOnlineSessionSearch> LobbySearch;
	
	FDelegateHandle CreateLobbyCompleteDelegateHandle;
	FDelegateHandle FindLobbiesCompleteDelegateHandle;
};
