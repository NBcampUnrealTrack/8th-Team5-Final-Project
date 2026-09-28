// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "HBOnlineSessionSubsystem.generated.h"

class UNetDriver;

namespace ENetworkFailure
{
	enum Type : int;
}
namespace ETravelFailure
{
	enum Type : int;
}

UENUM(BlueprintType)
enum class EHBMatchStartState : uint8
{
	Idle,
	Publishing,
	Traveling,
	Failed
};

UENUM(BlueprintType)
enum class EHBLobbyOperationState : uint8
{
	Idle,
	Creating,
	Finding,
	Joining,
	InLobby,
	Failed,
	OpeningHostMap,
	CleaningUpHost
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHBOnMatchStartStateChanged,
	EHBMatchStartState,
	State,
	const FString&,
	ErrorMessage
	);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHBOnLobbyOperationStateChanged,
	EHBLobbyOperationState,
	State,
	const FString&,
	ErrorMessage
	);

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
	
	bool CreateLobby(int32 MaxPlayers);
	bool FindLobbies();
	bool JoinLobby(int32 InSearchResultIndex);
	void LeaveLobby();

	UFUNCTION(BlueprintCallable, Category = "Online|Lobby")
	bool RequestCreateLobby(int32 MaxPlayers);

	/** Starts an asynchronous Steam listen host. True means accepted, not ready.
	 * Observe OnLobbyOperationStateChanged; InLobby means the listen map is ready.
	 * MaxPlayers includes the host. LobbyMap must be a saved map asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Online|Lobby")
	bool RequestHostLobby(int32 MaxPlayers, TSoftObjectPtr<UWorld> LobbyMap);

	UFUNCTION(BlueprintCallable, Category = "Online|Lobby")
	bool RequestFindAndJoinLobby();

	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	EHBLobbyOperationState GetLobbyOperationState() const
	{
		return LobbyOperationState;
	}

	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	bool IsLobbyOperationInProgress() const
	{
		return LobbyOperationState == EHBLobbyOperationState::Creating ||
			LobbyOperationState == EHBLobbyOperationState::Finding ||
			LobbyOperationState == EHBLobbyOperationState::Joining ||
			LobbyOperationState == EHBLobbyOperationState::OpeningHostMap ||
			LobbyOperationState == EHBLobbyOperationState::CleaningUpHost;
	}

	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	FString GetLastLobbyOperationError() const
	{
		return LastLobbyOperationError;
	}

	UPROPERTY(BlueprintAssignable, Category = "Online|Lobby")
	FHBOnLobbyOperationStateChanged OnLobbyOperationStateChanged;
	
	bool PublishMatchServer(const FString& ServerAddress);
	void ConnectToMatchServer(const FString& ServerAddress);
	void ConnectToPublishMatchServer();
	
	UFUNCTION(BlueprintCallable, Category = "Online|Match")
	bool RequestStartMatch();

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	EHBMatchStartState GetMatchStartState() const { return MatchStartState; }

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	bool IsMatchStartInProgress() const
	{
		return MatchStartState == EHBMatchStartState::Publishing ||
			MatchStartState == EHBMatchStartState::Traveling;
	}

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	FString GetLastMatchStartError() const { return LastMatchStartError; }

	UPROPERTY(BlueprintAssignable, Category = "Online|Match")
	FHBOnMatchStartStateChanged OnMatchStartStateChanged;

	bool StartMatchAsLobbyOwner(const FString& ServerAddress);
	
private:
	void HandleHostSessionCreated(FName SessionName, bool bWasSuccessful);
	void HandleHostMapLoaded(UWorld* LoadedWorld);
	void HandleHostTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void FailHostRequest(const FString& Error);
	bool BeginHostCleanup(float DeltaSeconds);
	void HandleHostCleanupComplete(FName SessionName, bool bWasSuccessful);
	FString PendingHostMap;
	FString HostFailureReason;
	bool bHostRequestActive = false;
	FDelegateHandle HostCreateDelegateHandle;
	FDelegateHandle HostTravelFailureDelegateHandle;
	FDelegateHandle HostMapLoadedDelegateHandle;
	FTSTicker::FDelegateHandle HostCleanupTickerHandle;
	TWeakObjectPtr<UWorld> HostCleanupWorld;

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
	void HandleNetworkFailure(
		UWorld* InWorld,
		UNetDriver* InNetDriver,
		ENetworkFailure::Type InFailureType,
		const FString& InErrorString
		);
	bool TravelToJoinedDedicatedServer(FName InSessionName);
	void StartMatchServerPolling();
	void StopMatchServerPolling();
	void PollMatchServerSettings();
	bool ResolveMatchServerAddress(
		const FString& RequestedAddress,
		FString& OutServerAddress
		) const;
	bool ValidateMatchServerAddress(
		const FString& ServerAddress,
		FString& OutNormalizedAddress,
		FString& OutError
		) const;
	bool FailMatchStart(const FString& ErrorMessage);
	void SetMatchStartState(
		EHBMatchStartState NewState,
		const FString& ErrorMessage = FString()
		);
	bool FailLobbyOperation(const FString& ErrorMessage);
	void SetLobbyOperationState(
		EHBLobbyOperationState NewState,
		const FString& ErrorMessage = FString()
		);
	
	IOnlineSessionPtr SessionInterface;

	EHBLobbyOperationState LobbyOperationState = EHBLobbyOperationState::Idle;
	FString LastLobbyOperationError;
	bool bJoinFirstLobbyAfterFind = false;
	
	EHBMatchStartState MatchStartState = EHBMatchStartState::Idle;
	FString LastMatchStartError;

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
	FDelegateHandle NetworkFailureDelegateHandle;
	FTimerHandle MatchServerPollTimerHandle;
};
