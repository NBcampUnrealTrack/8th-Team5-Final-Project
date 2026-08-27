// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Interfaces/OnlineIdentityInterface.h"

namespace HBOnline
{
	const FName PartyLobbySessionName(TEXT("HBPartyLobbySession"));
	
	const FName MatchServerKey(TEXT("HB_MATCH_SERVER"));
	const FName MatchReadyKey(TEXT("HB_MATCH_READY"));
}

void UHBOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
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

void UHBOnlineSessionSubsystem::CreateLobby(int32 MaxPlayers)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | SessionInterface invalid")
			);
		
		return;
	}
	
	PendingMatchServerAddress.Reset();
	bConnectAfterLobbyUpdate = false;
	bMatchTravelRequested = false;
	
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
		
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | CreateLobby request failed")
			);
		
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | CreateLobby requested")
		);
}

void UHBOnlineSessionSubsystem::FindLobbies()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Online | Lobby | FindLobbies failed: SessionInterface is invalid"));
		return;
	}

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

		UE_LOG(LogTemp, 
			Warning,
			TEXT("Online | Lobby | FindLobbies request failed to start")
			);

		return;
	}

	UE_LOG(LogTemp, 
		Warning,
		TEXT("Online | Lobby | FindLobbies requested")
		);
}



void UHBOnlineSessionSubsystem::JoinLobby(int32 InSearchResultIndex)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Join failed: SessionInterface invalid")
		);
		
		return;
	}

	if (!LobbySearch.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Join failed: LobbySearch invalid")
		);
		
		return;
	}

	if (!LobbySearch->SearchResults.IsValidIndex(InSearchResultIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Join failed: Invalid result index %d"),
			InSearchResultIndex
		);
		
		return;
	}

	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) != nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Join failed: Already has GameSession")
		);
		
		return;
	}

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

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | JoinLobby request failed")
		);
		
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | JoinLobby requested | Index: %d"),
		InSearchResultIndex
	);
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
	if (ServerAddress.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect failed: ServerAddress is empty")
			);
		
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect failed: World unavailable")
			);
		
		return;
	}
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Connect failed: Local PlayerController unavailable")
			);
		
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Match | Connecting to Dedicated Server: %s"),
		*ServerAddress
		);
	
	PlayerController->ClientTravel(
		ServerAddress,
		ETravelType::TRAVEL_Absolute
		);
}

void UHBOnlineSessionSubsystem::ConnectToPublishMatchServer()
{
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

	ConnectToMatchServer(ServerAddress);
}

void UHBOnlineSessionSubsystem::StartMatchAsLobbyOwner(const FString& ServerAddress)
{
	if (!SessionInterface.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: SessionInterface invalid")
        );

        return;
    }

    if (ServerAddress.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: ServerAddress empty")
        );

        return;
    }

    if (bConnectAfterLobbyUpdate || bMatchTravelRequested)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Online | Match | Start ignored: Match travel already requested")
        );

        return;
    }

    FNamedOnlineSession* LobbySession =
        SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName);

    if (LobbySession == nullptr || !LobbySession->OwningUserId.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: Party Lobby or Owner unavailable")
        );

        return;
    }

    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();

    if (OnlineSubsystem == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: OnlineSubsystem unavailable")
        );

        return;
    }

    IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();

    if (!IdentityInterface.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: IdentityInterface unavailable")
        );

        return;
    }

    const TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInterface->GetUniquePlayerId(0);

    if (!LocalUserId.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Start failed: Local User ID unavailable")
        );

        return;
    }

    if (*LocalUserId != *LobbySession->OwningUserId)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Online | Match | Start rejected: Local user is not Lobby Owner")
        );

        return;
    }

    PendingMatchServerAddress = ServerAddress;
    bConnectAfterLobbyUpdate = true;

    if (!PublishMatchServer(ServerAddress))
    {
        PendingMatchServerAddress.Reset();
        bConnectAfterLobbyUpdate = false;
    }
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

    if (ServerAddress.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Online | Match | Publish failed: ServerAddress empty")
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
        ServerAddress,
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
        *ServerAddress
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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Created: %s"),
			*InSessionName.ToString()
			);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | Create failed: %s"),
			*InSessionName.ToString()
			);
	}
}

void UHBOnlineSessionSubsystem::HandleFindLobbiesComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
	}

	if (!bWasSuccessful || !LobbySearch.IsValid())
	{
		UE_LOG(LogTemp, 
			Warning,
			TEXT("Online | Lobby | FindLobbies failed")
			);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				20.0f,
				FColor::Red,
				TEXT("Find Lobby FAILED")
			);
		}

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
		LobbySettings.Reset();
		
		PendingMatchServerAddress.Reset();
		bConnectAfterLobbyUpdate = false;
		bMatchTravelRequested = false;
		
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Left : %s"),
			*InSessionName.ToString()
			);
	}
	else
	{
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Lobby update | Session: %s | Success: False"),
			*InSessionName.ToString()
		);

		PendingMatchServerAddress.Reset();
		bConnectAfterLobbyUpdate = false;

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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Start failed: Pending server address empty")
		);

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
		return;
	}

	FString ServerAddress;

	if (!InSessionSettings.Get(HBOnline::MatchServerKey,ServerAddress))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Lobby updated but server address does not exist")
		);

		return;
	}

	if (ServerAddress.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Match | Lobby updated but server address is empty")
		);

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
