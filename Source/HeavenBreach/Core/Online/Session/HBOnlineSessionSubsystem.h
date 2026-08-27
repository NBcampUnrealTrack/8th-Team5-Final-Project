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
	void JoinLobby(int32 InSearchResultIndex);
	void LeaveLobby();
	
	bool PublishMatchServer(const FString& ServerAddress);
	void ConnectToMatchServer(const FString& ServerAddress);
	void ConnectToPublishMatchServer();
	
	void StartMatchAsLobbyOwner(const FString& ServerAddress);
	
private:
	void HandleCreateLobbyComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindLobbiesComplete(bool bWasSuccessful);
	void HandleJoinLobbyComplete(
		FName InSessionName,
		EOnJoinSessionCompleteResult::Type InResult
		);
	void HandleLeaveLobbyComplete(
		FName InSessionName,
		bool bWasSuccessful
		);
	void HandleUpdateLobbyComplete(FName InSessionName, bool bWasSuccessful);
	void HandleSessionSettingsUpdated(
		FName InSessionName, 
		const FOnlineSessionSettings& InSessionSettings);
	
	IOnlineSessionPtr SessionInterface;
	
	FString PendingMatchServerAddress;
	bool bConnectAfterLobbyUpdate = false;
	
	bool bMatchTravelRequested = false;
	
	TSharedPtr<FOnlineSessionSettings> LobbySettings;
	TSharedPtr<FOnlineSessionSearch> LobbySearch;
	
	FDelegateHandle CreateLobbyCompleteDelegateHandle;
	FDelegateHandle FindLobbiesCompleteDelegateHandle;
	FDelegateHandle JoinLobbyCompleteDelegateHandle;
	FDelegateHandle LeaveLobbyCompleteDelegateHandle;
	FDelegateHandle UpdateLobbyCompleteDelegateHandle;
	FDelegateHandle SessionSettingsUpdatedDelegateHandle;
};
