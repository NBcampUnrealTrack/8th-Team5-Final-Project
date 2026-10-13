// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "HBOnlineSessionSubsystem.generated.h"

class UNetDriver;
class FOnlineFriend;
class FOnlineUserPresence;
class IOnlinePresence;
struct FWorldContext;

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
	CleaningUpJoin,
	Leaving
};

/** Map-independent lifecycle of a personal Steam friends-only listen hub. */
UENUM(BlueprintType)
enum class EHBInviteHubState : uint8
{
	Disabled,
	WaitingForSteam,
	PreparingHost,
	Hosting,
	// Existing Blueprint names cover both invitations and friend session switches.
	LeavingForInvite,
	JoiningInvite,
	Guest,
	InMatch,
	Returning,
	Suspended,
	Failed
};

/** Snapshot of the latest search. SessionId remains the selection key across refreshes. */
USTRUCT(BlueprintType)
struct FHBLobbySearchEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) FString RoomName;
	UPROPERTY(BlueprintReadOnly) bool bPasswordProtected = false;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) FString MapPackage;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 OpenSlots = 0;
	UPROPERTY(BlueprintReadOnly) int32 PingInMs = 0;
};

USTRUCT(BlueprintType)
struct FHBSteamFriendEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString UserId;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	/** Existing Steam HB_STATE value: Lobby or Match. Other states are excluded. */
	UPROPERTY(BlueprintReadOnly) FString State;
	UPROPERTY(BlueprintReadOnly) bool bCanInvite = false;
	UPROPERTY(BlueprintReadOnly) bool bCanJoin = false;
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
	/** Called by the authoritative lobby GameMode; clients never create a personal session here. */
	void NotifyInviteHubEntered(UWorld* HubWorld);
	UFUNCTION(BlueprintPure, Category = "Online|Hub")
	EHBInviteHubState GetInviteHubState() const { return InviteHubState; }
	UFUNCTION(BlueprintPure, Category = "Online|Hub")
	FString GetInviteHubError() const { return InviteHubError; }
	UFUNCTION(BlueprintPure, Category = "Online|Hub")
	bool IsAutomaticInviteHubEnabled() const { return bAutomaticInviteHubEnabled; }
	UFUNCTION(BlueprintPure, Category = "Online|Hub")
	bool IsLocalLobbyHost() const;
	/** Called after results or when leaving a party; this API does not skip the result screen. */
	UFUNCTION(BlueprintCallable, Category = "Online|Hub")
	bool RequestReturnToOwnHub(const FString& Notice = TEXT(""));
	UFUNCTION(BlueprintCallable, Category = "Online|Hub")
	bool RetryInviteHub();
	FString GetOwnHubMap() const { return OwnHubMap; }

	bool RequestHostRoom(const FString& Name, const FString& Password, TSoftObjectPtr<UWorld> LobbyMap);
	bool RequestJoinRoom(const FString& SessionId, const FString& Password);
	bool RefreshSteamFriends();
	TArray<FHBSteamFriendEntry> GetSteamFriends() const;
	bool IsReadingFriends() const { return bReadingFriends; }
	bool InviteSteamFriend(const FString& UserId);
	/** Finds this friend's current lobby without leaving ours. True means the lookup was accepted. */
	UFUNCTION(BlueprintCallable, Category = "Online|Friends")
	bool RequestJoinSteamFriend(const FString& UserId);
	bool GetPendingInvite(FHBLobbySearchEntry& Entry) const;
	bool AcceptPendingInvite(const FString& Password);
	void DeclinePendingInvite();
	void SetLobbyReturnMap(const FString& Package) { if (!bAutomaticInviteHubEnabled) LobbyReturnMap = Package; }
	bool RequestLeaveToMenu(const FString& Notice = FString());
	FString ConsumeMenuNotice();
	FString GetHostedRoomName() const { return HostedRoomName; }
	FString CheckLobbyPassword(const FString& Options) const;
	void PublishGamePresence(const FString& State);
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
		return FindFriendSessionCompleteHandle.IsValid() || IsServerTravelInProgress() || LobbyOperationState == EHBLobbyOperationState::Creating ||
			LobbyOperationState == EHBLobbyOperationState::Finding ||
			LobbyOperationState == EHBLobbyOperationState::Joining ||
			LobbyOperationState == EHBLobbyOperationState::OpeningHostMap ||
			LobbyOperationState == EHBLobbyOperationState::CleaningUpHost ||
			LobbyOperationState == EHBLobbyOperationState::ConnectingToLobby ||
			LobbyOperationState == EHBLobbyOperationState::CleaningUpJoin ||
			LobbyOperationState == EHBLobbyOperationState::Leaving;
	}

	UFUNCTION(BlueprintPure, Category = "Online|Lobby")
	FString GetLastLobbyOperationError() const
	{
		return AuxiliaryError.IsEmpty() ? LastLobbyOperationError : AuxiliaryError;
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
	bool RequestHostLobbyInternal(int32 MaxPlayers, TSoftObjectPtr<UWorld> LobbyMap, const FString& Name, const FString& Password, bool bFriendsOnlyHub = false);
	void SetInviteHubState(EHBInviteHubState State, const FString& Error = FString());
	bool TickInviteHub(float DeltaSeconds);
	bool CanSwitchHubSession() const;
	bool BeginHubSessionSwitch(const FOnlineSessionSearchResult& Result);
	bool ValidateHubSession(const FOnlineSessionSearchResult& Result);
	void NotifyInviteHubSessionReady(bool bHost);
	void NotifyInviteHubSessionLeft();
	bool bAutomaticInviteHubEnabled = false;
	FString OwnHubMap;
	FString HostedHubProof;
	FString PendingHubJoinProof;
	EHBInviteHubState InviteHubState = EHBInviteHubState::Disabled;
	FString InviteHubError;
	double SteamReadyDeadline = 0.0;
	TSharedPtr<FOnlineSessionSearchResult> QueuedHubSession;
	FTSTicker::FDelegateHandle InviteHubTickerHandle;
	friend class FHBInviteHubGuardsTest;
	bool JoinLobbyResult(const FOnlineSessionSearchResult& Result, const FString& Password);
	void InitializeMultiplayerLobby();
	void DeinitializeMultiplayerLobby();
	void HandleSteamInvite(bool bSuccess, int32 LocalUser, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
	static bool ReadSteamFriendPresence(const FOnlineUserPresence& Presence, FHBSteamFriendEntry& Entry, FString* ExcludedReason = nullptr);
	void UpdateSteamFriend(const FOnlineFriend& Friend, const FOnlineUserPresence& Presence);
	void HandleSteamFriendPresenceQueried(uint64 RequestId, bool bSuccess);
	void CompleteSteamFriendsRefresh(uint64 RequestId, bool bSuccess);
	bool TickSteamPresence(float DeltaSeconds);
	void HandleGamePresencePublished(uint64 RequestId, const FString& State, bool bSuccess, const FOnlineUserPresence* Presence);
	static bool HasGamePresence(const FOnlineUserPresence& Presence, const FString& State);
	TSharedPtr<FOnlineFriend> FindSteamFriend(const FString& UserId) const;
	bool CanInviteSteamFriends() const;
	void HandleFindFriendSessionComplete(int32 LocalUser, bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results, uint64 RequestId);
	void ResetFriendSessionSearch(bool bCancelSearch = false);
	bool ReturnToMenu(float DeltaSeconds);
	FString HostedRoomName;
	FString HostedPasswordProof;
	FString HostedPasswordSalt;
	FString PendingJoinProof;
	FString ConnectedLobbyMap;
	FString LobbyReturnMap;
	FString MenuNotice;
	bool bReturnAfterLeave = false;
	bool bReadingFriends = false;
	bool bSteamFriendsStale = false;
	bool bFriendPresenceQueryFailed = false;
	int32 PendingFriendPresenceQueries = 0;
	uint64 FriendsReadRequestId = 0;
	double FriendsReadDeadline = 0.0;
	TArray<TSharedRef<FOnlineFriend>> PendingSteamFriends;
	TSharedPtr<IOnlinePresence, ESPMode::ThreadSafe> SteamPresenceInterface;
	FDelegateHandle SteamPresenceReceivedHandle;
	FTSTicker::FDelegateHandle SteamPresenceTickerHandle;
	FString DesiredGamePresence;
	uint64 GamePresenceRequestId = 0;
	bool bGamePresencePending = false;
	double GamePresenceDeadline = 0.0;
	FString AuxiliaryError;
	TArray<FHBSteamFriendEntry> SteamFriends;
	TSharedPtr<FOnlineSessionSearchResult> PendingInvite;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle FindFriendSessionCompleteHandle;
	int32 FriendSessionLocalUser = INDEX_NONE;
	uint64 FriendSessionRequestId = 0;
	TSharedPtr<const FUniqueNetId> PendingFriendId;
	FString PendingFriendLobbyId;
	FDelegateHandle LobbySeamlessTravelHandle;
	FTSTicker::FDelegateHandle ReturnMenuTickerHandle;
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
	void HandleLocalMapDeparture(const FWorldContext& WorldContext, const FString& MapName);
	void ResetLobbyAfterLeave();
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
	FDelegateHandle LocalMapDepartureDelegateHandle;
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
