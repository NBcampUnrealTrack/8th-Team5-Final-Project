// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

namespace HBOnline
{
	const FName PartyLobbySessionName(TEXT("HBPartyLobbySession"));
	
	const FName MatchServerKey(TEXT("HB_MATCH_SERVER"));
	const FName MatchReadyKey(TEXT("HB_MATCH_READY"));

	const TCHAR* MatchServerCommandLineKey = TEXT("HBMatchServer=");
	const TCHAR* LocalMatchServerAddress = TEXT("127.0.0.1:17777");
	constexpr float MatchServerPollIntervalSeconds = 0.25f;
}

void UHBOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine != nullptr)
	{
		NetworkFailureDelegateHandle =
			GEngine->OnNetworkFailure().AddUObject(
				this,
				&UHBOnlineSessionSubsystem::HandleNetworkFailure
				);
	}
	
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	
	if (OnlineSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | OnlineSubsystem is not Available")
			);
		
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | Subsystem: %s"),
		*OnlineSubsystem->GetSubsystemName().ToString()
		);
	
	SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | SessionInterface is not Available")
			);
		
		return;
	}
	
	SessionSettingsUpdatedDelegateHandle = 
		SessionInterface->AddOnSessionSettingsUpdatedDelegate_Handle(
			FOnSessionSettingsUpdatedDelegate::CreateUObject(
				this,
				&UHBOnlineSessionSubsystem::HandleSessionSettingsUpdated)
				);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | SessionInterface is initialized")
		);
}

void UHBOnlineSessionSubsystem::Deinitialize()
{
	StopMatchServerPolling();

	if (GEngine != nullptr && NetworkFailureDelegateHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
	}

	NetworkFailureDelegateHandle.Reset();

	if (SessionInterface.IsValid() && SessionSettingsUpdatedDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnSessionSettingsUpdatedDelegate_Handle(
			SessionSettingsUpdatedDelegateHandle
			);
	}
	
	SessionSettingsUpdatedDelegateHandle.Reset();
	
	SessionInterface.Reset();
	
	Super::Deinitialize();
}

bool UHBOnlineSessionSubsystem::RequestCreateLobby(int32 MaxPlayers)
{
	if (IsLobbyOperationInProgress())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Create ignored: Operation already in progress")
			);

		return false;
	}

	if (SessionInterface.IsValid() &&
		SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) != nullptr)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		return true;
	}

	return CreateLobby(MaxPlayers);
}

bool UHBOnlineSessionSubsystem::RequestFindAndJoinLobby()
{
	if (IsLobbyOperationInProgress())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Find and join ignored: Operation already in progress")
			);

		return false;
	}

	if (SessionInterface.IsValid() &&
		SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) != nullptr)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		return true;
	}

	bJoinFirstLobbyAfterFind = true;
	return FindLobbies();
}

bool UHBOnlineSessionSubsystem::CreateLobby(int32 MaxPlayers)
{
	if (!SessionInterface.IsValid())
	{
		return FailLobbyOperation(TEXT("Create failed: SessionInterface invalid"));
	}

	if (IsLobbyOperationInProgress())
	{
		return false;
	}

	if (MaxPlayers < 1)
	{
		return FailLobbyOperation(TEXT("Create failed: MaxPlayers must be positive"));
	}

	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) != nullptr)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		return true;
	}

	bJoinFirstLobbyAfterFind = false;
	SetLobbyOperationState(EHBLobbyOperationState::Creating);
	
	PendingMatchServerAddress.Reset();
	bConnectAfterLobbyUpdate = false;
	bMatchTravelRequested = false;
	SetMatchStartState(EHBMatchStartState::Idle);
	
	LobbySettings = MakeShared<FOnlineSessionSettings>();
	
	LobbySettings->NumPublicConnections = MaxPlayers;
	LobbySettings->NumPrivateConnections = 0;
	
	LobbySettings->bIsLANMatch = false;
	LobbySettings->bShouldAdvertise = true;
	LobbySettings->bAllowJoinInProgress = true;
	
	LobbySettings->bUsesPresence = true;
	LobbySettings->bAllowJoinViaPresence = true;
	LobbySettings->bAllowInvites = true;
	
	LobbySettings->bUseLobbiesIfAvailable = true;
	
	LobbySettings->Set(
		FName(TEXT("HB_LOBBY")),
		FString(TEXT("1")),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
		);
	
	LobbySettings->Set(
		HBOnline::MatchReadyKey,
		false,
		EOnlineDataAdvertisementType::ViaOnlineService
		);
	
	LobbySettings->Set(
		HBOnline::MatchServerKey,
		FString(),
		EOnlineDataAdvertisementType::ViaOnlineService
		);
	
	const FOnCreateSessionCompleteDelegate Delegate = 
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this,
			&UHBOnlineSessionSubsystem::HandleCreateLobbyComplete
			);
	
	CreateLobbyCompleteDelegateHandle = 
		SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(Delegate);
	
	const bool bRequestStarted = 
		SessionInterface->CreateSession(
			0,
			HBOnline::PartyLobbySessionName,
			*LobbySettings
			);
	
	if (!bRequestStarted)
	{
		SessionInterface
		->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteDelegateHandle);
		
		CreateLobbyCompleteDelegateHandle.Reset();
		
		return FailLobbyOperation(TEXT("CreateLobby request failed to start"));
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | CreateLobby requested")
		);

	return true;
}

bool UHBOnlineSessionSubsystem::FindLobbies()
{
	if (!SessionInterface.IsValid())
	{
		return FailLobbyOperation(TEXT("Find failed: SessionInterface invalid"));
	}

	if (LobbyOperationState == EHBLobbyOperationState::Creating ||
		LobbyOperationState == EHBLobbyOperationState::Joining)
	{
		return false;
	}

	SetLobbyOperationState(EHBLobbyOperationState::Finding);

	LobbySearch = MakeShared<FOnlineSessionSearch>();

	LobbySearch->MaxSearchResults = 50;
	LobbySearch->bIsLanQuery = false;

	// Steam Lobby 방식만 검색
	LobbySearch->QuerySettings.Set(
		SEARCH_LOBBIES,
		true,
		EOnlineComparisonOp::Equals
	);
	
	
	// Spacewar(AppID 480)의 다른 Lobby와 구분
	LobbySearch->QuerySettings.Set(
		FName(TEXT("HB_LOBBY")),
		FString(TEXT("1")),
		EOnlineComparisonOp::Equals
	);
	
	FindLobbiesCompleteDelegateHandle =
		SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
			FOnFindSessionsCompleteDelegate::CreateUObject(
				this,
				&UHBOnlineSessionSubsystem::HandleFindLobbiesComplete
			)
		);

	const bool bStarted =
		SessionInterface->FindSessions(
			0,
			LobbySearch.ToSharedRef()
		);

	if (!bStarted)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);

		LobbySearch.Reset();

		return FailLobbyOperation(TEXT("FindLobbies request failed to start"));
	}

	UE_LOG(LogTemp, 
		Warning,
		TEXT("Online | Lobby | FindLobbies requested")
		);

	return true;
}



bool UHBOnlineSessionSubsystem::JoinLobby(int32 InSearchResultIndex)
{
	if (!SessionInterface.IsValid())
	{
		return FailLobbyOperation(TEXT("Join failed: SessionInterface invalid"));
	}

	if (LobbyOperationState == EHBLobbyOperationState::Creating ||
		LobbyOperationState == EHBLobbyOperationState::Joining)
	{
		return false;
	}

	if (!LobbySearch.IsValid())
	{
		return FailLobbyOperation(TEXT("Join failed: LobbySearch invalid"));
	}

	if (!LobbySearch->SearchResults.IsValidIndex(InSearchResultIndex))
	{
		return FailLobbyOperation(FString::Printf(
			TEXT("Join failed: Invalid result index %d"),
			InSearchResultIndex
			));
	}

	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) != nullptr)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		return true;
	}

	SetLobbyOperationState(EHBLobbyOperationState::Joining);

	const FOnJoinSessionCompleteDelegate JoinCompleteDelegate =
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this,
			&UHBOnlineSessionSubsystem::HandleJoinLobbyComplete
		);

	JoinLobbyCompleteDelegateHandle =
		SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinCompleteDelegate);

	const bool bRequestStarted =
		SessionInterface->JoinSession(
			0,
			HBOnline::PartyLobbySessionName,
			LobbySearch->SearchResults[InSearchResultIndex]
		);

	if (!bRequestStarted)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteDelegateHandle);

		JoinLobbyCompleteDelegateHandle.Reset();

		return FailLobbyOperation(TEXT("JoinLobby request failed to start"));
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | JoinLobby requested | Index: %d"),
		InSearchResultIndex
	);

	return true;
}

void UHBOnlineSessionSubsystem::LeaveLobby()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed : SessionInterface invalid")
			);
		
		return;
	}
	
	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed : GameSession does not exist")
			);
		
		return;
	}
	
	const FOnDestroySessionCompleteDelegate LeaveCompleteDelegate =
		FOnDestroySessionCompleteDelegate::CreateUObject(
			this,
			&UHBOnlineSessionSubsystem::HandleLeaveLobbyComplete
			);
	
	LeaveLobbyCompleteDelegateHandle = 
		SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(LeaveCompleteDelegate);
	
	const bool bRequestStarted = 
		SessionInterface->DestroySession(HBOnline::PartyLobbySessionName);
	
	if (!bRequestStarted)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveLobbyCompleteDelegateHandle);
		
		LeaveLobbyCompleteDelegateHandle.Reset();
		
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | LeaveLobby request failed")
			);
		
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | LeaveLobby requested")
		);
}

void UHBOnlineSessionSubsystem::ConnectToMatchServer(const FString& ServerAddress)
{
	FString NormalizedServerAddress;
	FString ValidationError;

	if (!ValidateMatchServerAddress(
		ServerAddress,
		NormalizedServerAddress,
		ValidationError))
	{
		FailMatchStart(FString::Printf(
			TEXT("Connect failed: %s | Address: %s"),
			*ValidationError,
			*ServerAddress
			));
		
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		FailMatchStart(TEXT("Connect failed: World unavailable"));
		
		return;
	}
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		FailMatchStart(TEXT("Connect failed: Local PlayerController unavailable"));
		
		return;
	}

	StopMatchServerPolling();
	bMatchTravelRequested = true;
	SetMatchStartState(EHBMatchStartState::Traveling);
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Connecting to Dedicated Server: %s"),
		*NormalizedServerAddress
		);

	// Lobby UIOnly mode sets the persistent GameViewportClient to ignore game input.
	// Restore native game input before travel; CommonUI's Game config does not clear that flag.
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->SetShowMouseCursor(false);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Gameplay input restored before travel")
		);
	
	PlayerController->ClientTravel(
		NormalizedServerAddress,
		ETravelType::TRAVEL_Absolute
		);
}

void UHBOnlineSessionSubsystem::ConnectToPublishMatchServer()
{
	if (bConnectAfterLobbyUpdate || bMatchTravelRequested)
	{
		return;
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect published failed: SessionInterface invalid")
		);

		return;
	}

	FNamedOnlineSession* LobbySession =
		SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName);

	if (LobbySession == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect published failed: Party Lobby does not exist")
		);

		return;
	}

	bool bMatchReady = false;

	if (!LobbySession->SessionSettings.Get(
			HBOnline::MatchReadyKey,
			bMatchReady))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Match | Connect published failed: MatchReady not found")
		);

		return;
	}

	if (!bMatchReady)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Match | Connect published failed: Match is not ready")
		);

		return;
	}

	FString ServerAddress;

	if (!LobbySession->SessionSettings.Get(
			HBOnline::MatchServerKey,
			ServerAddress))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect published failed: MatchServer not found")
		);

		return;
	}

	if (ServerAddress.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect published failed: MatchServer is empty")
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Published server found: %s"),
		*ServerAddress
	);

	bMatchTravelRequested = true;
	StopMatchServerPolling();
	ConnectToMatchServer(ServerAddress);
}

bool UHBOnlineSessionSubsystem::RequestStartMatch()
{
	return StartMatchAsLobbyOwner(FString());
}

bool UHBOnlineSessionSubsystem::StartMatchAsLobbyOwner(const FString& ServerAddress)
{
	if (IsMatchStartInProgress() ||
		bConnectAfterLobbyUpdate ||
		bMatchTravelRequested)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Match | Start ignored: Match travel already requested")
			);

		return false;
	}

	if (!SessionInterface.IsValid())
	{
		return FailMatchStart(TEXT("Start failed: SessionInterface invalid"));
	}

	FString ResolvedServerAddress;

	if (!ResolveMatchServerAddress(ServerAddress, ResolvedServerAddress))
	{
		return FailMatchStart(
			TEXT("Start failed: Match server address unavailable")
			);
	}

	FNamedOnlineSession* LobbySession =
		SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName);

	if (LobbySession == nullptr || !LobbySession->OwningUserId.IsValid())
	{
		return FailMatchStart(
			TEXT("Start failed: Party Lobby or Owner unavailable")
			);
	}

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();

	if (OnlineSubsystem == nullptr)
	{
		return FailMatchStart(TEXT("Start failed: OnlineSubsystem unavailable"));
	}

	IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();

	if (!IdentityInterface.IsValid())
	{
		return FailMatchStart(TEXT("Start failed: IdentityInterface unavailable"));
	}

	const TSharedPtr<const FUniqueNetId> LocalUserId =
		IdentityInterface->GetUniquePlayerId(0);

	if (!LocalUserId.IsValid())
	{
		return FailMatchStart(TEXT("Start failed: Local User ID unavailable"));
	}

	if (*LocalUserId != *LobbySession->OwningUserId)
	{
		return FailMatchStart(
			TEXT("Start rejected: Local user is not Lobby Owner")
			);
	}

	SetMatchStartState(EHBMatchStartState::Publishing);
	PendingMatchServerAddress = ResolvedServerAddress;
	bConnectAfterLobbyUpdate = true;

	if (!PublishMatchServer(ResolvedServerAddress))
	{
		return FailMatchStart(
			TEXT("Start failed: Match server publish request failed")
			);
	}

	return true;
}

bool UHBOnlineSessionSubsystem::FailLobbyOperation(const FString& ErrorMessage)
{
	bJoinFirstLobbyAfterFind = false;

	const FString ResolvedErrorMessage = ErrorMessage.IsEmpty()
		? TEXT("Unknown lobby operation error")
		: ErrorMessage;

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Online | Lobby | Failed: %s"),
		*ResolvedErrorMessage
		);

	SetLobbyOperationState(
		EHBLobbyOperationState::Failed,
		ResolvedErrorMessage
		);

	return false;
}

void UHBOnlineSessionSubsystem::SetLobbyOperationState(
	EHBLobbyOperationState NewState,
	const FString& ErrorMessage)
{
	const FString ResolvedErrorMessage =
		NewState == EHBLobbyOperationState::Failed
			? ErrorMessage
			: FString();

	if (LobbyOperationState == NewState &&
		LastLobbyOperationError == ResolvedErrorMessage)
	{
		return;
	}

	LobbyOperationState = NewState;
	LastLobbyOperationError = ResolvedErrorMessage;

	OnLobbyOperationStateChanged.Broadcast(
		LobbyOperationState,
		LastLobbyOperationError
		);
}

bool UHBOnlineSessionSubsystem::FailMatchStart(const FString& ErrorMessage)
{
	StopMatchServerPolling();
	PendingMatchServerAddress.Reset();
	bConnectAfterLobbyUpdate = false;
	bMatchTravelRequested = false;

	const FString ResolvedErrorMessage = ErrorMessage.IsEmpty()
		? TEXT("Unknown match start error")
		: ErrorMessage;

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Online | Match | Failed: %s"),
		*ResolvedErrorMessage
		);

	SetMatchStartState(
		EHBMatchStartState::Failed,
		ResolvedErrorMessage
		);

	return false;
}

void UHBOnlineSessionSubsystem::SetMatchStartState(
	EHBMatchStartState NewState,
	const FString& ErrorMessage)
{
	const FString ResolvedErrorMessage =
		NewState == EHBMatchStartState::Failed
			? ErrorMessage
			: FString();

	if (MatchStartState == NewState &&
		LastMatchStartError == ResolvedErrorMessage)
	{
		return;
	}

	MatchStartState = NewState;
	LastMatchStartError = ResolvedErrorMessage;

	OnMatchStartStateChanged.Broadcast(
		MatchStartState,
		LastMatchStartError
		);
}

bool UHBOnlineSessionSubsystem::PublishMatchServer(const FString& ServerAddress)
{
	if (!SessionInterface.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Publish failed: SessionInterface invalid")
        );

        return false;
    }

	FString NormalizedServerAddress;
	FString ValidationError;

	if (!ValidateMatchServerAddress(
		ServerAddress,
		NormalizedServerAddress,
		ValidationError))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Publish failed: %s | Address: %s"),
			*ValidationError,
			*ServerAddress
        );

        return false;
    }

    FNamedOnlineSession* LobbySession =
        SessionInterface->GetNamedSession(
            HBOnline::PartyLobbySessionName
        );

    if (LobbySession == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Publish failed: Party Lobby does not exist")
        );

        return false;
    }

    FOnlineSessionSettings UpdatedSettings =
        LobbySession->SessionSettings;

    UpdatedSettings.Set(
        HBOnline::MatchServerKey,
        NormalizedServerAddress,
        EOnlineDataAdvertisementType::ViaOnlineService
    );

    UpdatedSettings.Set(
        HBOnline::MatchReadyKey,
        true,
        EOnlineDataAdvertisementType::ViaOnlineService
    );

    const FOnUpdateSessionCompleteDelegate UpdateCompleteDelegate =
        FOnUpdateSessionCompleteDelegate::CreateUObject(
            this,
            &UHBOnlineSessionSubsystem::HandleUpdateLobbyComplete
        );

    UpdateLobbyCompleteDelegateHandle =
        SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
            UpdateCompleteDelegate
        );

    const bool bRequestStarted =
        SessionInterface->UpdateSession(
            HBOnline::PartyLobbySessionName,
            UpdatedSettings,
            true
        );

    if (!bRequestStarted)
    {
        SessionInterface
            ->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteDelegateHandle);

        UpdateLobbyCompleteDelegateHandle.Reset();

        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Publish request failed")
        );

        return false;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Online | Match | Publish requested | Server: %s"),
		*NormalizedServerAddress
    );

    return true;
}

void UHBOnlineSessionSubsystem::HandleCreateLobbyComplete(FName InSessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && CreateLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface
		->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteDelegateHandle);
	}
	
	CreateLobbyCompleteDelegateHandle.Reset();
	if (bWasSuccessful)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Created: %s"),
			*InSessionName.ToString()
			);
	}
	else
	{
		FailLobbyOperation(FString::Printf(
			TEXT("Create failed: %s"),
			*InSessionName.ToString()
			));
	}
}

void UHBOnlineSessionSubsystem::HandleFindLobbiesComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
	}

	FindLobbiesCompleteDelegateHandle.Reset();

	if (!bWasSuccessful || !LobbySearch.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				20.0f,
				FColor::Red,
				TEXT("Find Lobby FAILED")
			);
		}

		FailLobbyOperation(TEXT("FindLobbies failed"));

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | Find complete | Results: %d"),
		LobbySearch->SearchResults.Num()
	);
	
	const int32 ResultCount = LobbySearch->SearchResults.Num();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			20.0f,
			FColor::Yellow,
			FString::Printf(
				TEXT("Find Complete | Results: %d"),
				ResultCount
			)
		);
	}

	for (int32 Index = 0; Index < LobbySearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = LobbySearch->SearchResults[Index];

		const FString OwnerName =
			Result.Session.OwningUserName;

		const FString Message = FString::Printf(
			TEXT("[%d] Owner: %s"),
			Index,
			*OwnerName
		);

		UE_LOG(LogTemp, Warning,
			TEXT("Online | Lobby | %s"),
			*Message
		);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				20.0f,
				FColor::Yellow,
				Message
			);
		}

		
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Found [%d] | Owner: %s | SessionId: %s | Ping: %d | OpenSlots: %d"),
			Index,
			*Result.Session.OwningUserName,
			*Result.GetSessionIdStr(),
			Result.PingInMs,
			Result.Session.NumOpenPublicConnections
		);
	}

	const bool bShouldJoinFirstLobby = bJoinFirstLobbyAfterFind;
	bJoinFirstLobbyAfterFind = false;

	if (!bShouldJoinFirstLobby)
	{
		SetLobbyOperationState(EHBLobbyOperationState::Idle);
		return;
	}

	if (LobbySearch->SearchResults.IsEmpty())
	{
		FailLobbyOperation(TEXT("No matching Steam lobby found"));
		return;
	}

	JoinLobby(0);
}

void UHBOnlineSessionSubsystem::HandleJoinLobbyComplete(
	FName InSessionName,
	EOnJoinSessionCompleteResult::Type InResult)
{
	if (SessionInterface.IsValid() && JoinLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
			JoinLobbyCompleteDelegateHandle
		);
	}

	JoinLobbyCompleteDelegateHandle.Reset();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | Join complete | Session: %s | Result: %d"),
		*InSessionName.ToString(),
		static_cast<int32>(InResult)
	);

	const bool bSuccess =
		InResult == EOnJoinSessionCompleteResult::Success;

	if (bSuccess)
	{
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		StartMatchServerPolling();
	}
	else
	{
		FailLobbyOperation(FString::Printf(
			TEXT("Join failed | Result: %d"),
			static_cast<int32>(InResult)
			));
	}

	if (GEngine)
	{
		const FString Message = bSuccess
			? FString::Printf(
				TEXT("JOIN SUCCESS | %s"),
				*InSessionName.ToString()
			)
			: FString::Printf(
				TEXT("JOIN FAILED | Result: %d"),
				static_cast<int32>(InResult)
			);

		GEngine->AddOnScreenDebugMessage(
			-1,
			20.0f,
			bSuccess ? FColor::Green : FColor::Red,
			Message
		);
	}
}

void UHBOnlineSessionSubsystem::HandleLeaveLobbyComplete(FName InSessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && LeaveLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveLobbyCompleteDelegateHandle);
	}
	
	LeaveLobbyCompleteDelegateHandle.Reset();
	
	if (bWasSuccessful)
	{
		StopMatchServerPolling();
		LobbySettings.Reset();
		LobbySearch.Reset();
		bJoinFirstLobbyAfterFind = false;
		
		PendingMatchServerAddress.Reset();
		bConnectAfterLobbyUpdate = false;
		bMatchTravelRequested = false;
		SetLobbyOperationState(EHBLobbyOperationState::Idle);
		
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Left : %s"),
			*InSessionName.ToString()
			);
	}
	else
	{
		FailLobbyOperation(FString::Printf(
			TEXT("Leave failed: %s"),
			*InSessionName.ToString()
			));

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed: %s"),
			*InSessionName.ToString()
			);
	}
	
}

void UHBOnlineSessionSubsystem::HandleUpdateLobbyComplete(FName InSessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && UpdateLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface
			->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteDelegateHandle);
	}

	UpdateLobbyCompleteDelegateHandle.Reset();

	if (!bWasSuccessful)
	{
		FailMatchStart(FString::Printf(
			TEXT("Lobby update failed | Session: %s"),
			*InSessionName.ToString()
			));

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Lobby update | Session: %s | Success: True"),
		*InSessionName.ToString()
	);
	
	if (!bConnectAfterLobbyUpdate)
	{
		return;
	}

	bConnectAfterLobbyUpdate = false;

	const FString ServerAddress = PendingMatchServerAddress;

	PendingMatchServerAddress.Reset();

	if (ServerAddress.IsEmpty())
	{
		FailMatchStart(TEXT("Start failed: Pending server address empty"));

		return;
	}

	if (bMatchTravelRequested)
	{
		return;
	}

	bMatchTravelRequested = true;

	ConnectToMatchServer(ServerAddress);
}

void UHBOnlineSessionSubsystem::HandleSessionSettingsUpdated(FName InSessionName,
	const FOnlineSessionSettings& InSessionSettings)
{
	if (InSessionName != HBOnline::PartyLobbySessionName)
	{
		return;
	}

	bool bMatchReady = false;

	// 먼저 Settings에서 값을 읽어야 한다.
	if (!InSessionSettings.Get(HBOnline::MatchReadyKey,bMatchReady))
	{
		return;
	}

	// Match가 다시 Ready=false가 되면
	// 다음 Match에서 다시 Travel할 수 있도록 Reset.
	if (!bMatchReady)
	{
		bMatchTravelRequested = false;
		SetMatchStartState(EHBMatchStartState::Idle);
		return;
	}

	FString ServerAddress;

	if (!InSessionSettings.Get(HBOnline::MatchServerKey,ServerAddress))
	{
		FailMatchStart(
			TEXT("Lobby updated but server address does not exist")
			);

		return;
	}

	if (ServerAddress.IsEmpty())
	{
		FailMatchStart(TEXT("Lobby updated but server address is empty"));

		return;
	}

	/*
	 * Owner는 UpdateSession 완료 후
	 * HandleUpdateLobbyComplete()에서 직접 이동한다.
	 *
	 * Owner에게도 SettingsUpdated가 발생할 수 있으므로
	 * 여기서는 Owner의 중복 ClientTravel을 막는다.
	 */
	if (bConnectAfterLobbyUpdate || bMatchTravelRequested)
	{
		return;
	}

	bMatchTravelRequested = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Match server received from Lobby | Server: %s"),
		*ServerAddress
	);

	ConnectToMatchServer(ServerAddress);
}

void UHBOnlineSessionSubsystem::StartMatchServerPolling()
{
	// Steam refreshes the named session when lobby data changes, but unlike EOS it
	// does not broadcast OnSessionSettingsUpdated to game code.
	if (bConnectAfterLobbyUpdate || bMatchTravelRequested)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Polling failed to start: World unavailable")
			);

		return;
	}

	World->GetTimerManager().SetTimer(
		MatchServerPollTimerHandle,
		this,
		&UHBOnlineSessionSubsystem::PollMatchServerSettings,
		HBOnline::MatchServerPollIntervalSeconds,
		true
		);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Waiting for Steam Lobby match server update")
		);

	PollMatchServerSettings();
}

void UHBOnlineSessionSubsystem::StopMatchServerPolling()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchServerPollTimerHandle);
	}

	MatchServerPollTimerHandle.Invalidate();
}

void UHBOnlineSessionSubsystem::PollMatchServerSettings()
{
	if (!SessionInterface.IsValid())
	{
		StopMatchServerPolling();
		return;
	}

	FNamedOnlineSession* LobbySession =
		SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName);

	if (LobbySession == nullptr)
	{
		StopMatchServerPolling();
		return;
	}

	bool bMatchReady = false;
	if (LobbySession->SessionSettings.Get(
			HBOnline::MatchReadyKey,
			bMatchReady) &&
		bMatchReady)
	{
		ConnectToPublishMatchServer();
	}
}

void UHBOnlineSessionSubsystem::HandleNetworkFailure(
	UWorld* InWorld,
	UNetDriver* InNetDriver,
	ENetworkFailure::Type InFailureType,
	const FString& InErrorString)
{
	(void)InNetDriver;

	if (InWorld != nullptr && InWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	const bool bHasMatchTravelState =
		bMatchTravelRequested ||
		bConnectAfterLobbyUpdate ||
		!PendingMatchServerAddress.IsEmpty() ||
		MatchStartState == EHBMatchStartState::Publishing ||
		MatchStartState == EHBMatchStartState::Traveling;

	if (!bHasMatchTravelState)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Online | Match | Network failure | Type: %s | Error: %s"),
		ENetworkFailure::ToString(InFailureType),
		*InErrorString
		);

	PendingMatchServerAddress.Reset();
	bConnectAfterLobbyUpdate = false;
	bMatchTravelRequested = false;
	SetMatchStartState(EHBMatchStartState::Failed, InErrorString);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Travel state reset | Retry allowed")
		);
}

bool UHBOnlineSessionSubsystem::ResolveMatchServerAddress(
	const FString& RequestedAddress,
	FString& OutServerAddress) const
{
	FString CandidateAddress = RequestedAddress;
	CandidateAddress.TrimStartAndEndInline();

	const TCHAR* AddressSource = TEXT("Explicit");

	if (CandidateAddress.IsEmpty())
	{
		if (FParse::Value(
			FCommandLine::Get(),
			HBOnline::MatchServerCommandLineKey,
			CandidateAddress))
		{
			CandidateAddress.TrimStartAndEndInline();
			AddressSource = TEXT("CommandLine");
		}
		else
		{
			CandidateAddress = HBOnline::LocalMatchServerAddress;
			AddressSource = TEXT("LocalDefault");
		}
	}

	FString ValidationError;

	if (!ValidateMatchServerAddress(
		CandidateAddress,
		OutServerAddress,
		ValidationError))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Address resolution failed | Source: %s | Address: %s | Error: %s"),
			AddressSource,
			*CandidateAddress,
			*ValidationError
			);

		OutServerAddress.Reset();
		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Address resolved | Source: %s | Server: %s"),
		AddressSource,
		*OutServerAddress
		);

	return true;
}

bool UHBOnlineSessionSubsystem::ValidateMatchServerAddress(
	const FString& ServerAddress,
	FString& OutNormalizedAddress,
	FString& OutError) const
{
	OutNormalizedAddress.Reset();
	OutError.Reset();

	FString CandidateAddress = ServerAddress;
	CandidateAddress.TrimStartAndEndInline();

	if (CandidateAddress.IsEmpty())
	{
		OutError = TEXT("Address is empty");
		return false;
	}

	int32 PortSeparatorIndex = INDEX_NONE;
	if (!CandidateAddress.FindLastChar(TEXT(':'), PortSeparatorIndex))
	{
		OutError = TEXT("Port is missing; expected Host:Port");
		return false;
	}

	FString Host = CandidateAddress.Left(PortSeparatorIndex);
	FString PortText = CandidateAddress.Mid(PortSeparatorIndex + 1);
	Host.TrimStartAndEndInline();
	PortText.TrimStartAndEndInline();

	if (Host.IsEmpty())
	{
		OutError = TEXT("Host is empty");
		return false;
	}

	if (Host.Len() > 253)
	{
		OutError = TEXT("Host is too long");
		return false;
	}

	if (
		Host.Equals(TEXT("0.0.0.0"), ESearchCase::IgnoreCase) ||
		Host.Equals(TEXT("*"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Bind-all address cannot be used as a client destination");
		return false;
	}

	for (const TCHAR Character : Host)
	{
		const bool bAllowedCharacter =
			FChar::IsAlnum(Character) ||
			Character == TEXT('.') ||
			Character == TEXT('-') ||
			Character == TEXT('_');

		if (!bAllowedCharacter)
		{
			OutError = TEXT("Host contains an unsupported character; use an IPv4 address or hostname");
			return false;
		}
	}

	if (PortText.IsEmpty())
	{
		OutError = TEXT("Port is empty");
		return false;
	}

	for (const TCHAR Character : PortText)
	{
		if (!FChar::IsDigit(Character))
		{
			OutError = TEXT("Port must contain digits only");
			return false;
		}
	}

	const int64 Port = FCString::Atoi64(*PortText);
	if (Port < 1 || Port > 65535)
	{
		OutError = TEXT("Port must be between 1 and 65535");
		return false;
	}

	OutNormalizedAddress = FString::Printf(
		TEXT("%s:%lld"),
		*Host,
		Port
		);

	return true;
}
