// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

namespace HBOnline
{
	// PartyLobbySessionName is retained for compatibility with the old public
	// API, while normal clients now join the VM-owned dedicated session below.
	const FName PartyLobbySessionName(TEXT("HBPartyLobbySession"));
	const FName DedicatedServerSessionName(TEXT("HBDedicatedServerSession"));
	const FName ProjectKey(TEXT("HB_PROJECT"));
	const TCHAR* ProjectValue = TEXT("HeavenBreach");
	
	const FName MatchServerKey(TEXT("HB_MATCH_SERVER"));
	const FName MatchReadyKey(TEXT("HB_MATCH_READY"));

	const TCHAR* MatchServerCommandLineKey = TEXT("HBMatchServer=");
	const TCHAR* LocalMatchServerAddress = TEXT("127.0.0.1:17777");
	constexpr float MatchServerPollIntervalSeconds = 0.25f;

	bool IsHeavenBreachSession(const FOnlineSessionSettings& Settings)
	{
		FString AdvertisedProject;
		return Settings.Get(ProjectKey, AdvertisedProject) &&
			AdvertisedProject.Equals(ProjectValue, ESearchCase::CaseSensitive);
	}
}

void UHBOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HostMapLoadedDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UHBOnlineSessionSubsystem::HandleHostMapLoaded);

	if (GEngine != nullptr)
	{
		HostTravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(
			this, &UHBOnlineSessionSubsystem::HandleHostTravelFailure);
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
	bHostRequestActive = false;
	PendingHostMap.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(HostCleanupTickerHandle);
	HostCleanupTickerHandle.Reset();
	HostCleanupWorld.Reset();
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(HostMapLoadedDelegateHandle);
	HostMapLoadedDelegateHandle.Reset();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(HostTravelFailureDelegateHandle);
	}
	HostTravelFailureDelegateHandle.Reset();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostCreateDelegateHandle);
	}
	HostCreateDelegateHandle.Reset();

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

bool UHBOnlineSessionSubsystem::RequestHostLobby(int32 MaxPlayers, TSoftObjectPtr<UWorld> LobbyMap)
{
	// Reject overlapping operations without changing the active operation's state.
	if (IsLobbyOperationInProgress() || IsMatchStartInProgress() ||
		LeaveLobbyCompleteDelegateHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Online | Host | Request ignored: another operation is pending"));
		return false;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone)
	{
		return FailLobbyOperation(TEXT("Host requires a standalone game world; leave the current connection first"));
	}
	if (MaxPlayers < 1 || MaxPlayers > 250)
	{
		return FailLobbyOperation(TEXT("Host player count must be between 1 and 250, including the host"));
	}
	const FString MapPackage = LobbyMap.ToSoftObjectPath().GetLongPackageName();
	FString MapFilename;
	if (LobbyMap.IsNull() || !FPackageName::IsValidLongPackageName(MapPackage) ||
		!FPackageName::DoesPackageExist(MapPackage, &MapFilename) ||
		!MapFilename.EndsWith(FPackageName::GetMapPackageExtension()))
	{
		return FailLobbyOperation(TEXT("Host requires an existing saved map asset"));
	}
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) ||
		!SessionInterface.IsValid() || OSS->GetSessionInterface() != SessionInterface)
	{
		return FailLobbyOperation(TEXT("Host requires the initialized Steam online subsystem"));
	}
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	const int32 LocalUser = LocalPlayer ? LocalPlayer->GetControllerId() : INDEX_NONE;
	if (LocalUser < 0 || !Identity.IsValid() || Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn)
	{
		return FailLobbyOperation(TEXT("Host requires a local player logged into Steam"));
	}
	const TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUser);
	if (!UserId.IsValid() || !UserId->IsValid())
	{
		return FailLobbyOperation(TEXT("Host failed: Steam user ID unavailable"));
	}
	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) ||
		SessionInterface->GetNamedSession(HBOnline::DedicatedServerSessionName) ||
		SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return FailLobbyOperation(TEXT("Host rejected: an existing session must be left before creating another"));
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.Set(HBOnline::ProjectKey, FString(HBOnline::ProjectValue), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("HB_LOBBY")), FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("HB_MAP")), MapPackage, EOnlineDataAdvertisementType::ViaOnlineService);

	StopMatchServerPolling();
	bJoinFirstLobbyAfterFind = false;
	PendingHostMap = MapPackage;
	HostFailureReason.Reset();
	bHostRequestActive = true;
	HostCreateDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleHostSessionCreated));
	SetLobbyOperationState(EHBLobbyOperationState::Creating);
	UE_LOG(LogTemp, Log, TEXT("Online | Host | Creating Steam lobby | Players: %d | Map: %s"), MaxPlayers, *MapPackage);
	if (!SessionInterface->CreateSession(*UserId, HBOnline::PartyLobbySessionName, Settings))
	{
		// Some providers invoke the completion delegate synchronously on failure.
		if (HostCreateDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostCreateDelegateHandle);
			HostCreateDelegateHandle.Reset();
			FailHostRequest(TEXT("Steam lobby creation request could not start"));
		}
		return false;
	}
	return true;
}

void UHBOnlineSessionSubsystem::HandleHostSessionCreated(FName SessionName, bool bWasSuccessful)
{
	if (!bHostRequestActive || SessionName != HBOnline::PartyLobbySessionName || !HostCreateDelegateHandle.IsValid()) return;
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostCreateDelegateHandle);
	HostCreateDelegateHandle.Reset();
	if (!bWasSuccessful)
	{
		FailHostRequest(TEXT("Steam lobby creation failed"));
		return;
	}
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone || PendingHostMap.IsEmpty())
	{
		FailHostRequest(TEXT("Host world changed before Steam lobby creation completed"));
		return;
	}
	SetLobbyOperationState(EHBLobbyOperationState::OpeningHostMap);
	UE_LOG(LogTemp, Log, TEXT("Online | Host | Opening %s?listen"), *PendingHostMap);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*PendingHostMap), true, TEXT("listen"));
}

void UHBOnlineSessionSubsystem::HandleHostMapLoaded(UWorld* LoadedWorld)
{
	if (!bHostRequestActive || LobbyOperationState != EHBLobbyOperationState::OpeningHostMap ||
		!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()) return;
	const UNetDriver* Driver = LoadedWorld->GetNetDriver();
	if (UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName()) != PendingHostMap ||
		LoadedWorld->GetNetMode() != NM_ListenServer || !Driver ||
		Driver->GetClass()->GetPathName() != TEXT("/Script/SteamSockets.SteamSocketsNetDriver"))
	{
		FailHostRequest(TEXT("Host map did not open with an active SteamSockets listen driver"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Online | Host | Ready | Map: %s | Driver: %s"), *PendingHostMap, *Driver->GetClass()->GetName());
	bHostRequestActive = false;
	PendingHostMap.Reset();
	SetLobbyOperationState(EHBLobbyOperationState::InLobby);
}

void UHBOnlineSessionSubsystem::HandleHostTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (bHostRequestActive && LobbyOperationState == EHBLobbyOperationState::OpeningHostMap &&
		World && World->GetGameInstance() == GetGameInstance())
	{
		FailHostRequest(FString::Printf(TEXT("Host travel failed (%d): %s"), static_cast<int32>(FailureType), *Error));
	}
}

void UHBOnlineSessionSubsystem::FailHostRequest(const FString& Error)
{
	if (!bHostRequestActive || LobbyOperationState == EHBLobbyOperationState::CleaningUpHost) return;
	HostFailureReason = Error;
	PendingHostMap.Reset();
	HostCleanupWorld = GetWorld();
	SetLobbyOperationState(EHBLobbyOperationState::CleaningUpHost);
	UE_LOG(LogTemp, Warning, TEXT("Online | Host | Rolling back: %s"), *Error);
	// A listen-failure delegate runs inside UWorld::Listen, which still accesses
	// its driver after broadcasting. Defer destruction until that stack unwinds.
	HostCleanupTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::BeginHostCleanup));
}

bool UHBOnlineSessionSubsystem::BeginHostCleanup(float DeltaSeconds)
{
	HostCleanupTickerHandle.Reset();
	if (!bHostRequestActive) return false;
	// Close a partially initialized listen driver without choosing a UI/return map.
	if (UWorld* FailedWorld = HostCleanupWorld.Get(); GEngine && FailedWorld && FailedWorld->GetNetDriver())
	{
		GEngine->ShutdownWorldNetDriver(FailedWorld);
	}
	HostCleanupWorld.Reset();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName))
	{
		HandleHostCleanupComplete(HBOnline::PartyLobbySessionName, true);
		return false;
	}
	if (!SessionInterface->DestroySession(HBOnline::PartyLobbySessionName,
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleHostCleanupComplete)))
	{
		if (bHostRequestActive) HandleHostCleanupComplete(HBOnline::PartyLobbySessionName, false);
	}
	return false;
}

void UHBOnlineSessionSubsystem::HandleHostCleanupComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bHostRequestActive || SessionName != HBOnline::PartyLobbySessionName ||
		LobbyOperationState != EHBLobbyOperationState::CleaningUpHost) return;
	bHostRequestActive = false;
	FString Error = HostFailureReason;
	HostFailureReason.Reset();
	if (!bWasSuccessful) Error += TEXT("; session cleanup failed. Leave the existing session before retrying.");
	FailLobbyOperation(Error);
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
		SessionInterface->GetNamedSession(HBOnline::DedicatedServerSessionName) != nullptr)
	{
		// A successful named session can survive a return to the title. Reuse its
		// Steam-resolved address instead of creating a client-hosted lobby.
		return TravelToJoinedDedicatedServer(
			HBOnline::DedicatedServerSessionName
			);
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
	if (bHostRequestActive) return false;
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

	// The VM is the only host. Exclude peer-hosted Steam lobbies so every
	// physical player follows exactly the same client path.
	LobbySearch->QuerySettings.Set(
		SEARCH_DEDICATED_ONLY,
		true,
		EOnlineComparisonOp::Equals
	);
	
	
	// Keep the search hint, but validate returned metadata locally as Steam's
	// dedicated-server search does not reliably apply custom query settings.
	LobbySearch->QuerySettings.Set(
		HBOnline::ProjectKey,
		FString(HBOnline::ProjectValue),
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
	if (bHostRequestActive) return false;
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

	if (!HBOnline::IsHeavenBreachSession(
		LobbySearch->SearchResults[InSearchResultIndex].Session.SessionSettings))
	{
		return FailLobbyOperation(
			TEXT("Join rejected: Missing or mismatched HB_PROJECT marker"));
	}

	if (SessionInterface->GetNamedSession(HBOnline::DedicatedServerSessionName) != nullptr)
	{
		return TravelToJoinedDedicatedServer(
			HBOnline::DedicatedServerSessionName
			);
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
			HBOnline::DedicatedServerSessionName,
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
	// Creation/travel owns its rollback; do not race a second DestroySession.
	if (bHostRequestActive) return;
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed : SessionInterface invalid")
			);
		
		return;
	}
	
	// Prefer the new VM session, but retain the legacy lobby fallback so this
	// API does not strand sessions created by older callers.
	const FName SessionToDestroy =
		SessionInterface->GetNamedSession(HBOnline::DedicatedServerSessionName) != nullptr
			? HBOnline::DedicatedServerSessionName
			: HBOnline::PartyLobbySessionName;

	if (SessionInterface->GetNamedSession(SessionToDestroy) == nullptr)
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
		SessionInterface->DestroySession(SessionToDestroy);
	
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
	if (bHostRequestActive) return;
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
	if (bHostRequestActive) return false;
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

	const int32 UnfilteredResultCount = LobbySearch->SearchResults.Num();
	LobbySearch->SearchResults.RemoveAll(
		[](const FOnlineSessionSearchResult& SearchResult)
		{
			return !HBOnline::IsHeavenBreachSession(SearchResult.Session.SessionSettings);
		});
	UE_LOG(LogTemp, Log,
		TEXT("Online | Lobby | Project filter | Total: %d | HeavenBreach: %d | Excluded: %d"),
		UnfilteredResultCount, LobbySearch->SearchResults.Num(),
		UnfilteredResultCount - LobbySearch->SearchResults.Num());

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
		FailLobbyOperation(TEXT("No matching HeavenBreach Steam dedicated server found"));
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

		// Steam owns endpoint resolution. This avoids trusting a hardcoded VM IP
		// and supports whichever transport SteamNetDriver selected.
		if (!TravelToJoinedDedicatedServer(InSessionName))
		{
			return;
		}
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

bool UHBOnlineSessionSubsystem::TravelToJoinedDedicatedServer(
	FName InSessionName)
{
	if (!SessionInterface.IsValid())
	{
		return FailLobbyOperation(
			TEXT("Travel failed: SessionInterface invalid")
			);
	}

	FString ConnectString;
	const FNamedOnlineSession* JoinedSession = SessionInterface->GetNamedSession(InSessionName);
	if (JoinedSession == nullptr ||
		!HBOnline::IsHeavenBreachSession(JoinedSession->SessionSettings))
	{
		return FailLobbyOperation(
			TEXT("Travel rejected: Missing or mismatched HB_PROJECT marker"));
	}

	if (!SessionInterface->GetResolvedConnectString(
		InSessionName,
		ConnectString) || ConnectString.IsEmpty())
	{
		return FailLobbyOperation(FString::Printf(
			TEXT("Travel failed: Steam could not resolve session %s"),
			*InSessionName.ToString()
			));
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World != nullptr
		? World->GetFirstPlayerController()
		: nullptr;

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return FailLobbyOperation(
			TEXT("Travel failed: Local PlayerController unavailable")
			);
	}

	// Title UI uses UI-only input. Clear that persistent viewport state before
	// network travel so gameplay input works immediately in the server lobby.
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->SetShowMouseCursor(false);
	bMatchTravelRequested = true;
	SetLobbyOperationState(EHBLobbyOperationState::InLobby);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | Dedicated Server | ClientTravel via resolved Steam session: %s"),
		*InSessionName.ToString()
		);

	PlayerController->ClientTravel(
		ConnectString,
		ETravelType::TRAVEL_Absolute
		);

	return true;
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
	if (bHostRequestActive)
	{
		if (InWorld && LobbyOperationState == EHBLobbyOperationState::OpeningHostMap)
		{
			FailHostRequest(FString::Printf(TEXT("Host network failure: %s"), *InErrorString));
		}
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
