// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBGameSession.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/World.h"

namespace HBSession
{
	const FName ProjectKey(TEXT("HB_PROJECT"));
	const FName MapNameKey(TEXT("HB_MAP"));
	const TCHAR* ProjectValue = TEXT("HeavenBreach");
}

void AHBGameSession::RegisterServer()
{
	if (!IsRunningDedicatedServer())
	{
		Super::RegisterServer();
		return;
	}

	// A dedicated process advertises one authoritative session. Reusing the
	// inherited GameSession name also lets a new GameMode keep the same Steam
	// session after ServerTravel instead of creating a competing duplicate.
	
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Dedicated Server | OnlineSubsystem unavailable")
			);
		
		return;
	}
	
	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp,
			Error,
			TEXT("Online | Dedicated Server | SessionInterface unavailable")
			);
		
		return;
	}

	if (SessionInterface->GetNamedSession(SessionName) != nullptr)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Online | Dedicated Server | Reusing session after travel: %s"),
			*SessionName.ToString()
			);
		return;
	}
	
	FOnlineSessionSettings Settings;
	
	Settings.NumPublicConnections = 10;
	Settings.NumPrivateConnections = 0;
	
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = true;
	
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	
	Settings.bUsesPresence = false;
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowInvites = false;
	
	Settings.bUseLobbiesIfAvailable = false;
	
	Settings.Set(
		HBSession::ProjectKey,
		FString(HBSession::ProjectValue),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
		);

	// Expose the current map in Steam's server metadata for diagnostics and
	// future server-browser filtering without changing any map asset.
	if (const UWorld* World = GetWorld())
	{
		Settings.Set(
			HBSession::MapNameKey,
			World->GetMapName(),
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
			);
	}
	
	const FOnCreateSessionCompleteDelegate Delegate = 
		FOnCreateSessionCompleteDelegate::CreateUObject(
		this, 
		&AHBGameSession::HandleCreateSessionComplete
		);
	
	CreateSessionCompleteDelegateHandle = 
		SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(Delegate);
	
	const bool bRequestStarted = 
		SessionInterface->CreateSession(
			0,
			SessionName,
			Settings
			);
	
	if (!bRequestStarted)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Dedicated Server | CreateSession request failed")
			);
		
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | Dedicated Server | CreateSession requested: %s"),
		*SessionName.ToString()
		);
}

void AHBGameSession::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem != nullptr)
	{
		IOnlineSessionPtr SessionInterface = 
			OnlineSubsystem->GetSessionInterface();
		
		if (SessionInterface.IsValid() && CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface
				->ClearOnCreateSessionCompleteDelegate_Handle(
				CreateSessionCompleteDelegateHandle);
		}
	}
	
	CreateSessionCompleteDelegateHandle.Reset();
	
	if (bWasSuccessful)
	{
		UE_LOG(
		LogTemp,
		Log, 
		TEXT("Online | Dedicated Server | Session: %s | Created: true"),
		*InSessionName.ToString()
		);
	}
	else
	{
		UE_LOG(
		LogTemp,
		Error, 
		TEXT("Online | Dedicated Server | Session: %s | Created: false"),
		*InSessionName.ToString()
		);
	}
}
