// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBGameSession.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

namespace HBSession
{
	const FName MatchSessionName(TEXT("HBMatchSession"));
	const FName ProjectKey(TEXT("HB_PROJECT"));
}

void AHBGameSession::RegisterServer()
{
	Super::RegisterServer();
	
	if (!IsRunningDedicatedServer())
	{
		return;
	}
	
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
			TEXT("Online | Dedicated Server | Match Session already exists")
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
		FString(TEXT("HeavenBreach")),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
		);
	
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
			HBSession::MatchSessionName,
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
		TEXT("Online | Dedicated Server | CreateSession requested")
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
		*SessionName.ToString()
		);
	}
	else
	{
		UE_LOG(
		LogTemp,
		Error, 
		TEXT("Online | Dedicated Server | Session: %s | Created: false"),
		*SessionName.ToString()
		);
	}
}
