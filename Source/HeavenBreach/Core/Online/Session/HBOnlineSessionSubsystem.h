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
enum class EHBServerTravelState : uint8
{
	Idle,
	ClosingLobby,
	Traveling,
	RestoringLobby,
	Complete,
	Failed
};

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
	CleaningUpHost,
	ConnectingToLobby,
	CleaningUpJoin
};

/** Snapshot of the latest search. SessionId remains the selection key across refreshes. */
USTRUCT(BlueprintType)
struct FHBLobbySearchEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) FString MapPackage;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 OpenSlots = 0;
	UPROPERTY(BlueprintReadOnly) int32 PingInMs = 0;
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

	/** Server-only transition primitive. No UI readiness policy. True means accepted. */
	bool RequestServerMatchTravel(TSoftObjectPtr<UWorld> Destination);
	/** Retry restoring admission after a failed transition, only in the original lobby. */
	bool RetryLobbyAdmissionRecovery();
	bool IsMatchAdmissionClosed() const { return bMatchAdmissionClosed; }
	UFUNCTION(BlueprintPure, Category = "Online|Travel")
	EHBServerTravelState GetServerTravelState() const { return ServerTravelState; }
	UFUNCTION(BlueprintPure, Category = "Online|Travel")
	FString GetServerTravelError() const { return ServerTravelError; }
	bool IsServerTravelInProgress() const
	{
		return ServerTravelState == EHBServerTravelState::ClosingLobby ||
			ServerTravelState == EHBServerTravelState::Traveling ||
			ServerTravelState == EHBServerTravelState::RestoringLobby;
	}

	/** Legacy compatibility only: rejects creation without an explicit listen map. */
	bool CreateLobby(int32 MaxPlayers);
	/** Search only; completion is reported through OnLobbyOperationStateChanged. */
	UFUNCTION(BlueprintCallable, Category = "Online|Lobby")
	bool FindLobbies();
	bool JoinLobby(int32 InSearchResultIndex);
	UFUNCTION(BlueprintCallable, Category = "Online|Lobby")
	bool RequestJoinLobby(const FString& SessionId);
	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	TArray<FHBLobbySearchEntry> GetLobbySearchResults() const;
	void LeaveLobby();

	UFUNCTION(BlueprintCallable, Category = "Online|Lobby", meta = (DeprecatedFunction, DeprecationMessage = "Use RequestHostLobby with an explicit lobby map."))
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
		return IsServerTravelInProgress() || LobbyOperationState == EHBLobbyOperationState::Creating ||
			LobbyOperationState == EHBLobbyOperationState::Finding ||
			LobbyOperationState == EHBLobbyOperationState::Joining ||
			LobbyOperationState == EHBLobbyOperationState::OpeningHostMap ||
			LobbyOperationState == EHBLobbyOperationState::CleaningUpHost ||
			LobbyOperationState == EHBLobbyOperationState::ConnectingToLobby ||
			LobbyOperationState == EHBLobbyOperationState::CleaningUpJoin;
	}

	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	FString GetLastLobbyOperationError() const
	{
		return LastLobbyOperationError;
	}

	UPROPERTY(BlueprintAssignable, Category = "Online|Lobby")
	FHBOnLobbyOperationStateChanged OnLobbyOperationStateChanged;

	/** Retired dedicated-server APIs. Kept as rejecting stubs for existing callers. */
	bool PublishMatchServer(const FString& ServerAddress);
	void ConnectToMatchServer(const FString& ServerAddress);
	void ConnectToPublishMatchServer();

	UFUNCTION(BlueprintCallable, Category = "Online|Match", meta = (DeprecatedFunction, DeprecationMessage = "Use Lobby GameMode readiness/confirmation to start a listen match."))
	bool RequestStartMatch();

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	EHBMatchStartState GetMatchStartState() const { return MatchStartState; }

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	bool IsMatchStartInProgress() const
	{
		return false; // Compatibility only; the dedicated match-start pipeline is retired.
	}

	UFUNCTION(BlueprintPure, Category = "Online|Match")
	FString GetLastMatchStartError() const { return LastMatchStartError; }

	UPROPERTY(BlueprintAssignable, Category = "Online|Match")
	FHBOnMatchStartStateChanged OnMatchStartStateChanged;

	bool StartMatchAsLobbyOwner(const FString& ServerAddress);

private:
	friend class FHBServerTravelGuardsTest;
	void HandleTravelSessionUpdated(FName SessionName, bool bSuccess);
	void FailServerTravel(const FString& Error);
	bool ObserveServerTravel(float DeltaSeconds);
	void SetServerTravelState(EHBServerTravelState State, const FString& Error = FString());
	EHBServerTravelState ServerTravelState = EHBServerTravelState::Idle;
	FString ServerTravelError;
	FString TravelSourceMap;
	FString TravelDestinationMap;
	bool bMatchAdmissionClosed = false;
	double ServerTravelDeadline = 0.0;
	TSharedPtr<FOnlineSessionSettings> PreTravelSettings;
	FDelegateHandle TravelSessionUpdateHandle;
	FTSTicker::FDelegateHandle ServerTravelTickerHandle;
	bool FindLobbiesInternal(bool bJoinFirstResult);
	bool ValidateLobbyClient(int32& OutLocalUser);
	void FailJoinRequest(const FString& Error);
	bool BeginJoinCleanup(float DeltaSeconds);
	void HandleJoinCleanupComplete(FName SessionName, bool bWasSuccessful);
	bool bJoinRequestActive = false;
	FString JoinFailureReason;
	FTSTicker::FDelegateHandle JoinCleanupTickerHandle;
	void HandleHostSessionCreated(FName SessionName, bool bWasSuccessful);
	void HandleLobbyMapLoaded(UWorld* LoadedWorld);
	void HandleLobbyTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void FailHostRequest(const FString& Error);
	bool BeginHostCleanup(float DeltaSeconds);
	void HandleHostCleanupComplete(FName SessionName, bool bWasSuccessful);
	FString PendingHostMap;
	FString HostFailureReason;
	bool bHostRequestActive = false;
	FDelegateHandle HostCreateDelegateHandle;
	FDelegateHandle LobbyTravelFailureDelegateHandle;
	FDelegateHandle LobbyMapLoadedDelegateHandle;
	FTSTicker::FDelegateHandle HostCleanupTickerHandle;
	TWeakObjectPtr<UWorld> HostCleanupWorld;

	void HandleFindLobbiesComplete(bool bWasSuccessful);
	void HandleJoinLobbyComplete(
		FName InSessionName,
		EOnJoinSessionCompleteResult::Type InResult
		);
	void HandleLeaveLobbyComplete(
		FName InSessionName,
		bool bWasSuccessful
		);
	void HandleNetworkFailure(
		UWorld* InWorld,
		UNetDriver* InNetDriver,
		ENetworkFailure::Type InFailureType,
		const FString& InErrorString
		);
	bool TravelToJoinedLobby(FName InSessionName);
	bool RejectLegacyMatchRequest();
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

	TSharedPtr<FOnlineSessionSearch> LobbySearch;

	FDelegateHandle FindLobbiesCompleteDelegateHandle;
	FDelegateHandle JoinLobbyCompleteDelegateHandle;
	FDelegateHandle LeaveLobbyCompleteDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
};
