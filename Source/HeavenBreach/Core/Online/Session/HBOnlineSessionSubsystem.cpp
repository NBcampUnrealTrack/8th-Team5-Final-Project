// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

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
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | SessionInterface is initialized")
		);
}

void UHBOnlineSessionSubsystem::Deinitialize()
{
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
			NAME_GameSession,
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

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
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
			NAME_GameSession,
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
	
	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
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
		SessionInterface->DestroySession(NAME_GameSession);
	
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

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | Find complete | Results: %d"),
		LobbySearch->SearchResults.Num()
	);

	for (int32 Index = 0; Index < LobbySearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = LobbySearch->SearchResults[Index];

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

void UHBOnlineSessionSubsystem::HandleJoinLobbyComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult)
{
	if (SessionInterface.IsValid() && JoinLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteDelegateHandle);
	}

	JoinLobbyCompleteDelegateHandle.Reset();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | Join complete | Session: %s | Result: %d"),
		*InSessionName.ToString(),
		static_cast<int32>(InResult)
	);
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
