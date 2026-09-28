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
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace HBOnline
{
	// Hosts and participants use the same local name for the Steam listen lobby.
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

	bool IsListenLobbySettings(const FOnlineSessionSettings& Settings)
	{
		FString LobbyMarker;
		return IsHeavenBreachSession(Settings) &&
			Settings.Get(FName(TEXT("HB_LOBBY")), LobbyMarker) && LobbyMarker == TEXT("1") &&
			!Settings.bIsDedicated && !Settings.bIsLANMatch &&
			Settings.bUsesPresence && Settings.bUseLobbiesIfAvailable &&
			Settings.BuildUniqueId == GetBuildUniqueId();
	}

	bool IsJoinableListenLobby(const FOnlineSessionSearchResult& Result, const TSharedPtr<const FUniqueNetId>& LocalUser)
	{
		return LocalUser.IsValid() && LocalUser->IsValid() && Result.IsValid() &&
			IsListenLobbySettings(Result.Session.SessionSettings) &&
			Result.Session.SessionSettings.bAllowJoinInProgress && Result.Session.NumOpenPublicConnections > 0 &&
			Result.Session.OwningUserId.IsValid() && *Result.Session.OwningUserId != *LocalUser;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
namespace HBOnlineTests
{
	class FSessionInfo final : public FOnlineSessionInfo
	{
		FUniqueNetIdStringRef Id = FUniqueNetIdString::Create(TEXT("test-lobby"), FName(TEXT("TEST")));
	public:
		virtual const uint8* GetBytes() const override { return Id->GetBytes(); }
		virtual int32 GetSize() const override { return Id->GetSize(); }
		virtual bool IsValid() const override { return true; }
		virtual FString ToString() const override { return Id->ToString(); }
		virtual FString ToDebugString() const override { return ToString(); }
		virtual const FUniqueNetId& GetSessionId() const override { return *Id; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBListenLobbyFilterTest, "HeavenBreach.Online.ListenLobbyFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBListenLobbyFilterTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<const FUniqueNetId> LocalUser = FUniqueNetIdString::Create(TEXT("local"), FName(TEXT("TEST")));
	FOnlineSessionSearchResult Valid;
	Valid.Session.SessionInfo = MakeShared<HBOnlineTests::FSessionInfo>();
	Valid.Session.OwningUserId = FUniqueNetIdString::Create(TEXT("host"), FName(TEXT("TEST")));
	Valid.Session.NumOpenPublicConnections = 1;
	Valid.Session.SessionSettings.bUsesPresence = true;
	Valid.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	Valid.Session.SessionSettings.bAllowJoinInProgress = true;
	Valid.Session.SessionSettings.Set(HBOnline::ProjectKey, FString(HBOnline::ProjectValue), EOnlineDataAdvertisementType::ViaOnlineService);
	Valid.Session.SessionSettings.Set(FName(TEXT("HB_LOBBY")), FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineService);
	TestTrue(TEXT("Compatible remote lobby with a free slot is accepted"), HBOnline::IsJoinableListenLobby(Valid, LocalUser));
	auto Rejected = [&](const TCHAR* Reason, const FOnlineSessionSearchResult& Result)
	{
		TestFalse(Reason, HBOnline::IsJoinableListenLobby(Result, LocalUser));
	};
	auto Candidate = Valid;
	Candidate.Session.SessionSettings.Set(HBOnline::ProjectKey, FString(TEXT("OtherGame")), EOnlineDataAdvertisementType::ViaOnlineService);
	Rejected(TEXT("Other projects sharing AppId 480 are rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.Remove(FName(TEXT("HB_LOBBY")));
	Rejected(TEXT("Missing lobby marker is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.bIsDedicated = true;
	Rejected(TEXT("Dedicated server is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.bIsLANMatch = true;
	Rejected(TEXT("LAN result is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.bUseLobbiesIfAvailable = false;
	Rejected(TEXT("Mismatched Steam lobby/presence flags are rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.BuildUniqueId ^= 1;
	Rejected(TEXT("Different build is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionSettings.bAllowJoinInProgress = false;
	Rejected(TEXT("Closed lobby is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.NumOpenPublicConnections = 0;
	Rejected(TEXT("Full lobby is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.OwningUserId = LocalUser;
	Rejected(TEXT("Own lobby is rejected"), Candidate);
	Candidate = Valid; Candidate.Session.SessionInfo.Reset();
	Rejected(TEXT("Invalid session info is rejected"), Candidate);
	TestFalse(TEXT("Missing local identity is rejected"), HBOnline::IsJoinableListenLobby(Valid, nullptr));
	return true;
}
#endif

void UHBOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LobbyMapLoadedDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UHBOnlineSessionSubsystem::HandleLobbyMapLoaded);

	if (GEngine != nullptr)
	{
		LobbyTravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(
			this, &UHBOnlineSessionSubsystem::HandleLobbyTravelFailure);
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
	bJoinRequestActive = false;
	FTSTicker::GetCoreTicker().RemoveTicker(JoinCleanupTickerHandle);
	JoinCleanupTickerHandle.Reset();
	PendingHostMap.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(HostCleanupTickerHandle);
	HostCleanupTickerHandle.Reset();
	HostCleanupWorld.Reset();
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LobbyMapLoadedDelegateHandle);
	LobbyMapLoadedDelegateHandle.Reset();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(LobbyTravelFailureDelegateHandle);
	}
	LobbyTravelFailureDelegateHandle.Reset();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostCreateDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteDelegateHandle);
	}
	FindLobbiesCompleteDelegateHandle.Reset();
	JoinLobbyCompleteDelegateHandle.Reset();
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

void UHBOnlineSessionSubsystem::HandleLobbyMapLoaded(UWorld* LoadedWorld)
{
	if (bJoinRequestActive && LobbyOperationState == EHBLobbyOperationState::ConnectingToLobby &&
		LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		const UNetDriver* JoinDriver = LoadedWorld->GetNetDriver();
		if (LoadedWorld->GetNetMode() != NM_Client || !JoinDriver ||
			JoinDriver->GetClass()->GetPathName() != TEXT("/Script/SteamSockets.SteamSocketsNetDriver"))
		{
			FailJoinRequest(TEXT("Joined map did not establish a SteamSockets client connection"));
			return;
		}
		bJoinRequestActive = false;
		UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Connected to Steam listen host"));
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		return;
	}
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

void UHBOnlineSessionSubsystem::HandleLobbyTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (bJoinRequestActive && LobbyOperationState == EHBLobbyOperationState::ConnectingToLobby &&
		World && World->GetGameInstance() == GetGameInstance())
	{
		FailJoinRequest(FString::Printf(TEXT("Lobby travel failed (%d): %s"), static_cast<int32>(FailureType), *Error));
		return;
	}
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
	return FindLobbiesInternal(true);
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

bool UHBOnlineSessionSubsystem::ValidateLobbyClient(int32& OutLocalUser)
{
	OutLocalUser = INDEX_NONE;
	if (!GetWorld() || !GetWorld()->IsGameWorld() || GetWorld()->GetNetMode() != NM_Standalone)
		return FailLobbyOperation(TEXT("Lobby search/join requires a disconnected standalone game world"));
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !SessionInterface.IsValid() ||
		OSS->GetSessionInterface() != SessionInterface)
		return FailLobbyOperation(TEXT("Lobby search/join requires Steam"));
	const ULocalPlayer* Player = GetGameInstance()->GetFirstGamePlayer();
	const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	OutLocalUser = Player ? Player->GetControllerId() : INDEX_NONE;
	if (OutLocalUser < 0 || !Identity.IsValid() || Identity->GetLoginStatus(OutLocalUser) != ELoginStatus::LoggedIn)
		return FailLobbyOperation(TEXT("Lobby search/join requires a local player logged into Steam"));
	if (SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) ||
		SessionInterface->GetNamedSession(HBOnline::DedicatedServerSessionName) ||
		SessionInterface->GetNamedSession(NAME_GameSession))
		return FailLobbyOperation(TEXT("Leave the existing session before searching or joining another lobby"));
	return true;
}

bool UHBOnlineSessionSubsystem::FindLobbies()
{
	return FindLobbiesInternal(false);
}

bool UHBOnlineSessionSubsystem::FindLobbiesInternal(bool bJoinFirstResult)
{
	if (IsLobbyOperationInProgress() || IsMatchStartInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	bJoinFirstLobbyAfterFind = false;
	LobbySearch.Reset();
	int32 LocalUser;
	if (!ValidateLobbyClient(LocalUser)) return false;
	LobbySearch = MakeShared<FOnlineSessionSearch>();
	LobbySearch->MaxSearchResults = 50;
	LobbySearch->bIsLanQuery = false;
	LobbySearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	LobbySearch->QuerySettings.Set(HBOnline::ProjectKey, FString(HBOnline::ProjectValue), EOnlineComparisonOp::Equals);
	LobbySearch->QuerySettings.Set(FName(TEXT("HB_LOBBY")), FString(TEXT("1")), EOnlineComparisonOp::Equals);
	bJoinFirstLobbyAfterFind = bJoinFirstResult;
	FindLobbiesCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleFindLobbiesComplete));
	SetLobbyOperationState(EHBLobbyOperationState::Finding);
	UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Searching Steam listen lobbies"));
	if (!SessionInterface->FindSessions(LocalUser, LobbySearch.ToSharedRef()))
	{
		if (FindLobbiesCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
			FindLobbiesCompleteDelegateHandle.Reset();
			bJoinFirstLobbyAfterFind = false;
			LobbySearch.Reset();
			FailLobbyOperation(TEXT("Steam lobby search could not start"));
		}
		return false;
	}
	return true;
}

TArray<FHBLobbySearchEntry> UHBOnlineSessionSubsystem::GetLobbySearchResults() const
{
	TArray<FHBLobbySearchEntry> Entries;
	if (!LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done) return Entries;
	for (const FOnlineSessionSearchResult& Result : LobbySearch->SearchResults)
	{
		FHBLobbySearchEntry Entry;
		Entry.SessionId = Result.GetSessionIdStr();
		Entry.HostName = Result.Session.OwningUserName;
		Result.Session.SessionSettings.Get(FName(TEXT("HB_MAP")), Entry.MapPackage);
		Entry.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Entry.OpenSlots = Result.Session.NumOpenPublicConnections;
		Entry.PingInMs = Result.PingInMs;
		Entries.Add(MoveTemp(Entry));
	}
	return Entries;
}

bool UHBOnlineSessionSubsystem::RequestJoinLobby(const FString& SessionId)
{
	if (IsLobbyOperationInProgress() || IsMatchStartInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	if (SessionId.IsEmpty() || !LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done)
		return FailLobbyOperation(TEXT("Join requires a completed lobby search and a session ID"));
	const int32 Index = LobbySearch->SearchResults.IndexOfByPredicate(
		[&SessionId](const FOnlineSessionSearchResult& Result) { return Result.GetSessionIdStr() == SessionId; });
	if (Index == INDEX_NONE) return FailLobbyOperation(TEXT("Selected lobby is no longer in the latest search results"));
	return JoinLobby(Index);
}

bool UHBOnlineSessionSubsystem::JoinLobby(int32 InSearchResultIndex)
{
	if (IsLobbyOperationInProgress() || IsMatchStartInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	int32 LocalUser;
	if (!ValidateLobbyClient(LocalUser)) return false;
	if (!LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done ||
		!LobbySearch->SearchResults.IsValidIndex(InSearchResultIndex))
		return FailLobbyOperation(TEXT("Join requires a valid result from a completed lobby search"));
	const FOnlineSessionSearchResult Result = LobbySearch->SearchResults[InSearchResultIndex];
	const IOnlineIdentityPtr Identity = IOnlineSubsystem::Get()->GetIdentityInterface();
	const TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUser);
	if (!HBOnline::IsJoinableListenLobby(Result, UserId))
		return FailLobbyOperation(TEXT("Selected lobby is full, incompatible, self-owned, or not a HeavenBreach listen lobby"));
	bJoinRequestActive = true;
	JoinFailureReason.Reset();
	bJoinFirstLobbyAfterFind = false;
	JoinLobbyCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleJoinLobbyComplete));
	SetLobbyOperationState(EHBLobbyOperationState::Joining);
	UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Joining session %s"), *Result.GetSessionIdStr());
	if (!SessionInterface->JoinSession(LocalUser, HBOnline::PartyLobbySessionName, Result))
	{
		if (JoinLobbyCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteDelegateHandle);
			JoinLobbyCompleteDelegateHandle.Reset();
			FailJoinRequest(TEXT("Steam lobby join request could not start"));
		}
		return false;
	}
	return true;
}

void UHBOnlineSessionSubsystem::LeaveLobby()
{
	// Creation/travel owns its rollback; do not race a second DestroySession.
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return;
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed : SessionInterface invalid")
			);
		
		return;
	}
	
	// Retain cleanup support for legacy dedicated sessions during migration.
	// New hosts and participants both use PartyLobbySessionName.
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
	if (bHostRequestActive || bJoinRequestActive) return;
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
	if (bHostRequestActive || bJoinRequestActive) return false;
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
	if (!FindLobbiesCompleteDelegateHandle.IsValid() || LobbyOperationState != EHBLobbyOperationState::Finding) return;
	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteDelegateHandle);
	FindLobbiesCompleteDelegateHandle.Reset();
	const bool bAutoJoin = bJoinFirstLobbyAfterFind;
	bJoinFirstLobbyAfterFind = false;
	if (!bWasSuccessful || !LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done)
	{
		LobbySearch.Reset();
		FailLobbyOperation(TEXT("Steam lobby search failed"));
		return;
	}
	const ULocalPlayer* Player = GetGameInstance()->GetFirstGamePlayer();
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
	const TSharedPtr<const FUniqueNetId> UserId = Player && Identity.IsValid()
		? Identity->GetUniquePlayerId(Player->GetControllerId()) : nullptr;
	LobbySearch->SearchResults.RemoveAll([&UserId](const FOnlineSessionSearchResult& Result)
	{
		return !HBOnline::IsJoinableListenLobby(Result, UserId);
	});
	UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Search complete | Joinable: %d"), LobbySearch->SearchResults.Num());
	if (bAutoJoin && LobbySearch->SearchResults.IsEmpty())
	{
		FailLobbyOperation(TEXT("No joinable HeavenBreach Steam lobby found"));
		return;
	}
	if (bAutoJoin)
	{
		// Release the search state without broadcasting an intermediate Idle event:
		// listeners must not start a competing operation before the auto-join.
		LobbyOperationState = EHBLobbyOperationState::Idle;
		JoinLobby(0);
		return;
	}
	SetLobbyOperationState(EHBLobbyOperationState::Idle);
}

void UHBOnlineSessionSubsystem::HandleJoinLobbyComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult)
{
	if (!bJoinRequestActive || InSessionName != HBOnline::PartyLobbySessionName || !JoinLobbyCompleteDelegateHandle.IsValid()) return;
	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteDelegateHandle);
	JoinLobbyCompleteDelegateHandle.Reset();
	if (InResult != EOnJoinSessionCompleteResult::Success)
	{
		FailJoinRequest(FString::Printf(TEXT("Steam lobby join failed: %s"), LexToString(InResult)));
		return;
	}
	TravelToJoinedLobby(InSessionName);
}

bool UHBOnlineSessionSubsystem::TravelToJoinedLobby(FName InSessionName)
{
	const FNamedOnlineSession* Session = SessionInterface.IsValid() ? SessionInterface->GetNamedSession(InSessionName) : nullptr;
	FString ConnectString;
	if (!Session || !HBOnline::IsListenLobbySettings(Session->SessionSettings) ||
		!SessionInterface->GetResolvedConnectString(InSessionName, ConnectString) || ConnectString.IsEmpty())
	{
		FailJoinRequest(TEXT("Steam could not resolve the joined listen lobby address"));
		return false;
	}
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
	{
		FailJoinRequest(TEXT("Local player/world unavailable for lobby travel"));
		return false;
	}
	SetLobbyOperationState(EHBLobbyOperationState::ConnectingToLobby);
	UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Connecting through Steam-resolved session"));
	// Do not parse Steam endpoints as Host:Port or change any UI/input state here.
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	return true;
}

void UHBOnlineSessionSubsystem::FailJoinRequest(const FString& Error)
{
	if (!bJoinRequestActive || LobbyOperationState == EHBLobbyOperationState::CleaningUpJoin) return;
	JoinFailureReason = Error;
	SetLobbyOperationState(EHBLobbyOperationState::CleaningUpJoin);
	UE_LOG(LogTemp, Warning, TEXT("Online | Lobby | Cleaning up failed join: %s"), *Error);
	// The engine owns pending connections/travel failure recovery. Only remove
	// our OSS membership, after the provider/engine failure callback unwinds.
	JoinCleanupTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::BeginJoinCleanup));
}

bool UHBOnlineSessionSubsystem::BeginJoinCleanup(float DeltaSeconds)
{
	JoinCleanupTickerHandle.Reset();
	if (!bJoinRequestActive) return false;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName))
	{
		HandleJoinCleanupComplete(HBOnline::PartyLobbySessionName, true);
		return false;
	}
	if (!SessionInterface->DestroySession(HBOnline::PartyLobbySessionName,
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleJoinCleanupComplete)))
	{
		if (bJoinRequestActive) HandleJoinCleanupComplete(HBOnline::PartyLobbySessionName, false);
	}
	return false;
}

void UHBOnlineSessionSubsystem::HandleJoinCleanupComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bJoinRequestActive || SessionName != HBOnline::PartyLobbySessionName ||
		LobbyOperationState != EHBLobbyOperationState::CleaningUpJoin) return;
	bJoinRequestActive = false;
	FString Error = JoinFailureReason;
	JoinFailureReason.Reset();
	LobbySearch.Reset();
	if (!bWasSuccessful) Error += TEXT("; session cleanup failed. Leave the existing session before retrying.");
	FailLobbyOperation(Error);
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
	if (bJoinRequestActive)
	{
		if (InWorld && LobbyOperationState == EHBLobbyOperationState::ConnectingToLobby)
			FailJoinRequest(FString::Printf(TEXT("Lobby connection failed: %s"), *InErrorString));
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
