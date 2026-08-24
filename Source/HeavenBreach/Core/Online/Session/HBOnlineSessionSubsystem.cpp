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
	LobbySettings->bAllowJoinInProgress = true;
	
	LobbySettings->bUsesPresence = true;
	LobbySettings->bAllowJoinViaPresence = true;
	LobbySettings->bAllowInvites = true;
	
	LobbySettings->bUseLobbiesIfAvailable = true;
	
	LobbySettings->Set(
		FName(TEXT("HB_LOBBY")),
		true,
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | SessionInterface invalid")
			);
		
		return;
	}
	
	LobbySearch = MakeShared<FOnlineSessionSearch>();
	
	LobbySearch->MaxSearchResults = 100;
	LobbySearch->bIsLanQuery = false;
	
	LobbySearch->QuerySettings.Set(
		SEARCH_LOBBIES,
		true,
		EOnlineComparisonOp::Equals
		);
	
	LobbySearch->QuerySettings.Set(
		FName(TEXT("HB_LOBBY")),
		true,
		EOnlineComparisonOp::Equals
		);
	
	const FOnFindSessionsCompleteDelegate Delegate = 
		FOnFindSessionsCompleteDelegate::CreateUObject(
			this,
			&UHBOnlineSessionSubsystem::HandleFindLobbiesComplete
			);
	
	FindLobbiesCompleteDelegateHandle = 
		SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);
	
	const bool bRequestStarted =
		SessionInterface->FindSessions(
			0,
			LobbySearch.ToSharedRef()
			);
	
	if (!bRequestStarted)
	{
		SessionInterface
		->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
		
		FindLobbiesCompleteDelegateHandle.Reset();
		
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | FindLobbies request failed")
			);
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | FindLobbies requested"))
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
	if (SessionInterface.IsValid() && FindLobbiesCompleteDelegateHandle.IsValid())
	{
		SessionInterface
			->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
	}

	FindLobbiesCompleteDelegateHandle.Reset();

	if (!bWasSuccessful || !LobbySearch.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | Find failed")
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Online | Lobby | Find completed | Results: %d"),
		LobbySearch->SearchResults.Num()
	);

	for (int32 Index = 0; Index < LobbySearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = LobbySearch->SearchResults[Index];

		bool bIsHBLobby = false;

		Result.Session.SessionSettings.Get(
			FName(TEXT("HB_LOBBY")),
			bIsHBLobby
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Result[%d] | Owner: %s | Ping: %d | HB: %s"),
			Index,
			*Result.Session.OwningUserName,
			Result.PingInMs,
			bIsHBLobby ? TEXT("True") : TEXT("False")
		);
	}
}
