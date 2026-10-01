// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Core/Online/Session/HBLobbyProtocol.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace HBOnline
{
	// Hosts and participants use the same local name for the Steam listen lobby.
	const FName PartyLobbySessionName(TEXT("HBPartyLobbySession"));
	const FName ProjectKey(TEXT("HB_PROJECT"));
	const TCHAR* ProjectValue = TEXT("HeavenBreach");

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

	bool IsLocallyOwnedListenLobby(const FNamedOnlineSession* Session, const TSharedPtr<const FUniqueNetId>& LocalUser)
	{
		// Steam OSS sets OwningUserId when creating the lobby, but UE 5.7
		// leaves FNamedOnlineSession::bHosting at its default false value.
		return Session && Session->SessionInfo.IsValid() && Session->SessionInfo->IsValid() &&
			LocalUser.IsValid() && LocalUser->IsValid() &&
			Session->OwningUserId.IsValid() && Session->OwningUserId->IsValid() &&
			*Session->OwningUserId == *LocalUser && IsListenLobbySettings(Session->SessionSettings);
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

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBListenLobbyOwnershipTest, "HeavenBreach.Online.ListenLobbyOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBListenLobbyOwnershipTest::RunTest(const FString&)
{
	const TSharedPtr<const FUniqueNetId> LocalUser = FUniqueNetIdString::Create(TEXT("local"), FName(TEXT("TEST")));
	FOnlineSessionSettings Settings;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(HBOnline::ProjectKey, FString(HBOnline::ProjectValue), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("HB_LOBBY")), FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineService);
	FNamedOnlineSession Session(HBOnline::PartyLobbySessionName, Settings);
	Session.SessionInfo = MakeShared<HBOnlineTests::FSessionInfo>();
	Session.OwningUserId = LocalUser;
	TestFalse(TEXT("Steam fixture retains the default bHosting value"), Session.bHosting);
	TestTrue(TEXT("Actual owner can host with Steam's unset bHosting flag"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	Session.bHosting = true;
	Session.OwningUserId = FUniqueNetIdString::Create(TEXT("remote"), FName(TEXT("TEST")));
	TestFalse(TEXT("bHosting cannot authorize a foreign owner"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	Session.OwningUserId = LocalUser;
	TestFalse(TEXT("Missing local identity cannot own the lobby"), HBOnline::IsLocallyOwnedListenLobby(&Session, nullptr));
	TestFalse(TEXT("Missing session cannot authorize travel"), HBOnline::IsLocallyOwnedListenLobby(nullptr, LocalUser));
	Session.OwningUserId.Reset();
	TestFalse(TEXT("Missing owner is rejected"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	Session.OwningUserId = LocalUser;
	Session.SessionSettings.bIsDedicated = true;
	TestFalse(TEXT("Owned dedicated session is not a listen lobby"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	Session.SessionSettings.bIsDedicated = false;
	Session.SessionSettings.Set(HBOnline::ProjectKey, FString(TEXT("OtherGame")), EOnlineDataAdvertisementType::ViaOnlineService);
	TestFalse(TEXT("Owned session from another project is rejected"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	Session.SessionSettings = Settings;
	Session.SessionInfo.Reset();
	TestFalse(TEXT("Session without valid provider info is rejected"), HBOnline::IsLocallyOwnedListenLobby(&Session, LocalUser));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRetiredDedicatedAPIsTest, "HeavenBreach.Online.RetiredDedicatedAPIs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBRetiredDedicatedAPIsTest::RunTest(const FString& Parameters)
{
	// No world or OSS is initialized: retired calls must reject without touching either.
	UGameInstance* Instance = NewObject<UGameInstance>();
	UHBOnlineSessionSubsystem* Sessions = NewObject<UHBOnlineSessionSubsystem>(Instance);
	TestFalse(TEXT("Cannot publish a dedicated endpoint"), Sessions->PublishMatchServer(TEXT("127.0.0.1:17777")));
	TestFalse(TEXT("Cannot start via explicit dedicated endpoint"), Sessions->StartMatchAsLobbyOwner(TEXT("127.0.0.1:17777")));
	TestFalse(TEXT("Cannot start via legacy Blueprint API"), Sessions->RequestStartMatch());
	Sessions->ConnectToMatchServer(TEXT("127.0.0.1:17777"));
	Sessions->ConnectToPublishMatchServer();
	TestEqual(TEXT("Legacy failure is reported"), Sessions->GetMatchStartState(), EHBMatchStartState::Failed);
	TestFalse(TEXT("Legacy pipeline cannot remain busy"), Sessions->IsMatchStartInProgress());
	TestTrue(TEXT("Legacy error points to Lobby GameMode"), Sessions->GetLastMatchStartError().Contains(TEXT("Lobby GameMode")));
	TestEqual(TEXT("Legacy match calls do not change lobby operation state"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
	AddExpectedError(TEXT("CreateLobby is retired."), EAutomationExpectedErrorFlags::Contains, 2);
	TestFalse(TEXT("Mapless C++ creation is rejected"), Sessions->CreateLobby(4));
	TestFalse(TEXT("Mapless Blueprint creation is rejected"), Sessions->RequestCreateLobby(4));
	TestTrue(TEXT("Mapless creation explains replacement API"), Sessions->GetLastLobbyOperationError().Contains(TEXT("RequestHostLobby")));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBServerTravelGuardsTest, "HeavenBreach.Online.ServerTravelGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBServerTravelGuardsTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UHBOnlineSessionSubsystem* Sessions = NewObject<UHBOnlineSessionSubsystem>(Instance);
	TestFalse(TEXT("No server world cannot start travel"), Sessions->RequestServerMatchTravel(nullptr));
	TestEqual(TEXT("Invalid request records failure"), Sessions->GetServerTravelState(), EHBServerTravelState::Failed);
	TestFalse(TEXT("Preflight failure does not close admission"), Sessions->IsMatchAdmissionClosed());
	for (EHBServerTravelState State : { EHBServerTravelState::ClosingLobby, EHBServerTravelState::Traveling, EHBServerTravelState::RestoringLobby })
	{
		Sessions->ServerTravelState = State;
		TestTrue(TEXT("Transition blocks overlapping lobby operations"), Sessions->IsLobbyOperationInProgress());
		TestFalse(TEXT("Duplicate travel is rejected"), Sessions->RequestServerMatchTravel(nullptr));
		TestEqual(TEXT("Duplicate request preserves current transition"), Sessions->GetServerTravelState(), State);
	}
	Sessions->ServerTravelState = EHBServerTravelState::Failed;
	Sessions->bMatchAdmissionClosed = true;
	TestFalse(TEXT("Failed transition cannot retry before restoring admission"), Sessions->RequestServerMatchTravel(nullptr));
	TestFalse(TEXT("Recovery cannot run outside its source lobby"), Sessions->RetryLobbyAdmissionRecovery());
	TestTrue(TEXT("Unsafe recovery keeps admission closed"), Sessions->IsMatchAdmissionClosed());
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

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Online | SessionInterface is initialized")
		);
	InitializeMultiplayerLobby();
}

void UHBOnlineSessionSubsystem::Deinitialize()
{
	DeinitializeMultiplayerLobby();
	FTSTicker::GetCoreTicker().RemoveTicker(ServerTravelTickerHandle);
	ServerTravelTickerHandle.Reset();
	if (SessionInterface.IsValid()) SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(TravelSessionUpdateHandle);
	TravelSessionUpdateHandle.Reset();
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
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveLobbyCompleteDelegateHandle);
	}
	LeaveLobbyCompleteDelegateHandle.Reset();
	FindLobbiesCompleteDelegateHandle.Reset();
	JoinLobbyCompleteDelegateHandle.Reset();
	HostCreateDelegateHandle.Reset();

	if (GEngine != nullptr && NetworkFailureDelegateHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
	}

	NetworkFailureDelegateHandle.Reset();

	SessionInterface.Reset();

	Super::Deinitialize();
}

void UHBOnlineSessionSubsystem::SetServerTravelState(EHBServerTravelState State, const FString& Error)
{
	ServerTravelState = State;
	ServerTravelError = Error;
	if (State == EHBServerTravelState::ClosingLobby || State == EHBServerTravelState::Traveling || State == EHBServerTravelState::RestoringLobby)
		PublishGamePresence(TEXT("Busy"));
	else if (State == EHBServerTravelState::Failed && !bMatchAdmissionClosed && GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer)
		PublishGamePresence(TEXT("Lobby"));
	UE_LOG(LogTemp, Log, TEXT("Online | ServerTravel | State: %d | %s"), static_cast<int32>(State), *Error);
}

bool UHBOnlineSessionSubsystem::RequestServerMatchTravel(TSoftObjectPtr<UWorld> Destination)
{
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid() || bMatchAdmissionClosed) return false;
	UWorld* World = GetWorld();
	AHBLobbyGameMode* Mode = World ? World->GetAuthGameMode<AHBLobbyGameMode>() : nullptr;
	if (!Mode || World->GetNetMode() != NM_ListenServer || !Mode->bUseSeamlessTravel ||
		World->IsInSeamlessTravel() || !World->NextURL.IsEmpty())
	{
		SetServerTravelState(EHBServerTravelState::Failed, TEXT("Match travel requires an idle seamless listen lobby"));
		return false;
	}
	const FString Target = Destination.ToSoftObjectPath().GetLongPackageName();
	FString Filename;
	const FString Source = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (Destination.IsNull() || !FPackageName::IsValidLongPackageName(Target) || Target == Source ||
		!FPackageName::DoesPackageExist(Target, &Filename) || !Filename.EndsWith(FPackageName::GetMapPackageExtension()))
	{
		SetServerTravelState(EHBServerTravelState::Failed, TEXT("Destination must be an existing map different from the lobby"));
		return false;
	}
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const int32 LocalUser = LocalPlayer ? LocalPlayer->GetControllerId() : INDEX_NONE;
	const TSharedPtr<const FUniqueNetId> UserId = LocalUser >= 0 && Identity.IsValid() &&
		Identity->GetLoginStatus(LocalUser) == ELoginStatus::LoggedIn ? Identity->GetUniquePlayerId(LocalUser) : nullptr;
	const FNamedOnlineSession* Session = SessionInterface.IsValid() ? SessionInterface->GetNamedSession(HBOnline::PartyLobbySessionName) : nullptr;
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !SessionInterface.IsValid() ||
		OSS->GetSessionInterface() != SessionInterface || !HBOnline::IsLocallyOwnedListenLobby(Session, UserId))
	{
		SetServerTravelState(EHBServerTravelState::Failed, TEXT("No locally hosted Steam lobby session"));
		return false;
	}
	PreTravelSettings = MakeShared<FOnlineSessionSettings>(Session->SessionSettings);
	TravelSourceMap = Source;
	TravelDestinationMap = Target;
	bMatchAdmissionClosed = true; // Immediate server gate while Steam metadata updates asynchronously.
	FOnlineSessionSettings Closed = *PreTravelSettings;
	Closed.bAllowJoinInProgress = false;
	Closed.bAllowJoinViaPresence = false;
	Closed.bAllowJoinViaPresenceFriendsOnly = false;
	Closed.bAllowInvites = false;
	Closed.Set(FName(TEXT("HB_MAP")), Target, EOnlineDataAdvertisementType::ViaOnlineService);
	TravelSessionUpdateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleTravelSessionUpdated));
	SetServerTravelState(EHBServerTravelState::ClosingLobby);
	if (!SessionInterface->UpdateSession(HBOnline::PartyLobbySessionName, Closed, true))
	{
		if (TravelSessionUpdateHandle.IsValid()) HandleTravelSessionUpdated(HBOnline::PartyLobbySessionName, false);
		return false;
	}
	return true;
}

void UHBOnlineSessionSubsystem::HandleTravelSessionUpdated(FName SessionName, bool bSuccess)
{
	if (SessionName != HBOnline::PartyLobbySessionName || !TravelSessionUpdateHandle.IsValid()) return;
	SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(TravelSessionUpdateHandle);
	TravelSessionUpdateHandle.Reset();
	if (ServerTravelState == EHBServerTravelState::RestoringLobby)
	{
		if (bSuccess)
		{
			bMatchAdmissionClosed = false;
			PreTravelSettings.Reset();
		}
		SetServerTravelState(EHBServerTravelState::Failed, bSuccess ? ServerTravelError :
			ServerTravelError + TEXT("; lobby admission restore failed; retry recovery or leave the session"));
		return;
	}
	if (ServerTravelState != EHBServerTravelState::ClosingLobby) return;
	if (!bSuccess)
	{
		FailServerTravel(TEXT("Could not close Steam lobby admission"));
		return;
	}
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_ListenServer || World->IsInSeamlessTravel() ||
		!World->NextURL.IsEmpty() || UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TravelSourceMap)
	{
		FailServerTravel(TEXT("Lobby world changed before travel could start"));
		return;
	}
	SetServerTravelState(EHBServerTravelState::Traveling);
	ServerTravelDeadline = FPlatformTime::Seconds() + 120.0;
	// Observe from GameInstance lifetime: seamless travel destroys the lobby GameMode.
	ServerTravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::ObserveServerTravel), 0.1f);
	if (!World->ServerTravel(TravelDestinationMap, true)) FailServerTravel(TEXT("ServerTravel rejected the destination"));
}

bool UHBOnlineSessionSubsystem::ObserveServerTravel(float DeltaSeconds)
{
	if (ServerTravelState != EHBServerTravelState::Traveling) return false;
	if (FPlatformTime::Seconds() > ServerTravelDeadline)
	{
		ServerTravelTickerHandle.Reset();
		FailServerTravel(TEXT("Server travel exceeded 120 seconds; admission remains closed unless the source lobby can be restored"));
		return false;
	}
	UWorld* World = GetWorld();
	if (World && !World->IsInSeamlessTravel() && World->NextURL.IsEmpty() && World->HasBegunPlay() &&
		UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TravelDestinationMap)
	{
		ServerTravelTickerHandle.Reset();
		if (World->GetNetMode() != NM_ListenServer)
		{
			FailServerTravel(TEXT("Destination did not retain the listen server"));
			return false;
		}
		PreTravelSettings.Reset();
		SetServerTravelState(EHBServerTravelState::Complete);
		PublishGamePresence(TEXT("Match"));
		return false;
	}
	return true;
}

void UHBOnlineSessionSubsystem::FailServerTravel(const FString& Error)
{
	FTSTicker::GetCoreTicker().RemoveTicker(ServerTravelTickerHandle);
	ServerTravelTickerHandle.Reset();
	SetServerTravelState(EHBServerTravelState::Failed, Error);
	// Do not reopen admission in a transition/game world after a partial travel.
	RetryLobbyAdmissionRecovery();
}

bool UHBOnlineSessionSubsystem::RetryLobbyAdmissionRecovery()
{
	UWorld* World = GetWorld();
	if (ServerTravelState != EHBServerTravelState::Failed || TravelSessionUpdateHandle.IsValid() ||
		!PreTravelSettings.IsValid() || !SessionInterface.IsValid() || !World ||
		World->GetNetMode() != NM_ListenServer || World->IsInSeamlessTravel() || !World->NextURL.IsEmpty() ||
		UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TravelSourceMap) return false;
	TravelSessionUpdateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::HandleTravelSessionUpdated));
	SetServerTravelState(EHBServerTravelState::RestoringLobby, ServerTravelError);
	if (!SessionInterface->UpdateSession(HBOnline::PartyLobbySessionName, *PreTravelSettings, true))
	{
		if (TravelSessionUpdateHandle.IsValid()) HandleTravelSessionUpdated(HBOnline::PartyLobbySessionName, false);
		return false;
	}
	return true;
}

bool UHBOnlineSessionSubsystem::RequestHostLobby(int32 MaxPlayers, TSoftObjectPtr<UWorld> LobbyMap)
{
	return RequestHostLobbyInternal(MaxPlayers, LobbyMap, TEXT("HeavenBreach"), FString());
}

bool UHBOnlineSessionSubsystem::RequestHostLobbyInternal(int32 MaxPlayers, TSoftObjectPtr<UWorld> LobbyMap, const FString& Name, const FString& Password)
{
	// Reject overlapping operations without changing the active operation's state.
	if (IsLobbyOperationInProgress() ||
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
	if (MaxPlayers < 1 || MaxPlayers > 4)
	{
		return FailLobbyOperation(TEXT("Host player count must be between 1 and 4, including the host"));
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
	HostedRoomName = Name.TrimStartAndEnd();
	HostedPasswordSalt = Password.IsEmpty() ? FString() : FGuid::NewGuid().ToString(EGuidFormats::Digits);
	HostedPasswordProof = HBLobbyProtocol::PasswordProof(Password, HostedPasswordSalt);
	Settings.Set(FName(TEXT("HB_ROOM")), HostedRoomName, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("HB_LOCKED")), FString(Password.IsEmpty() ? TEXT("0") : TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("HB_SALT")), HostedPasswordSalt, EOnlineDataAdvertisementType::ViaOnlineService);

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
	if (!bJoinRequestActive && LobbyOperationState == EHBLobbyOperationState::InLobby && LoadedWorld
		&& LoadedWorld->GetGameInstance() == GetGameInstance() && LoadedWorld->GetNetMode() == NM_Client)
	{
		const bool bInLobby = UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName()) == ConnectedLobbyMap;
		bMatchAdmissionClosed = !bInLobby;
		PublishGamePresence(bInLobby ? TEXT("Lobby") : TEXT("Match"));
		return;
	}
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
		ConnectedLobbyMap = UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName());
		PendingJoinProof.Reset();
		UE_LOG(LogTemp, Log, TEXT("Online | Lobby | Connected to Steam listen host"));
		SetLobbyOperationState(EHBLobbyOperationState::InLobby);
		PublishGamePresence(TEXT("Lobby"));
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
	PublishGamePresence(TEXT("Lobby"));
}

void UHBOnlineSessionSubsystem::HandleLobbyTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (ServerTravelState == EHBServerTravelState::Traveling && World && World->GetGameInstance() == GetGameInstance())
	{
		FailServerTravel(FString::Printf(TEXT("Server travel failed (%d): %s"), static_cast<int32>(FailureType), *Error));
		return;
	}
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
	if (!LobbyReturnMap.IsEmpty() && bWasSuccessful) RequestLeaveToMenu(Error);
}

bool UHBOnlineSessionSubsystem::RequestCreateLobby(int32 MaxPlayers)
{
	return CreateLobby(MaxPlayers);
}

bool UHBOnlineSessionSubsystem::RequestFindAndJoinLobby()
{
	return FindLobbiesInternal(true);
}

bool UHBOnlineSessionSubsystem::CreateLobby(int32 MaxPlayers)
{
	// Preserve old C++/Blueprint callers without creating an unhosted Steam lobby.
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	return FailLobbyOperation(TEXT("CreateLobby is retired. Use RequestHostLobby(MaxPlayers, LobbyMap) with an explicit map."));
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
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
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
		Result.Session.SessionSettings.Get(FName(TEXT("HB_ROOM")), Entry.RoomName);
		if (Entry.RoomName.IsEmpty()) Entry.RoomName = Entry.HostName;
		FString Locked;
		Result.Session.SessionSettings.Get(FName(TEXT("HB_LOCKED")), Locked);
		Entry.bPasswordProtected = Locked == TEXT("1");
		Result.Session.SessionSettings.Get(FName(TEXT("HB_MAP")), Entry.MapPackage);
		Entry.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Entry.OpenSlots = Result.Session.NumOpenPublicConnections;
		Entry.CurrentPlayers = FMath::Clamp(Entry.MaxPlayers - Entry.OpenSlots, 0, Entry.MaxPlayers);
		Entry.PingInMs = Result.PingInMs;
		Entries.Add(MoveTemp(Entry));
	}
	return Entries;
}

bool UHBOnlineSessionSubsystem::RequestJoinLobby(const FString& SessionId)
{
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	if (SessionId.IsEmpty() || !LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done)
		return FailLobbyOperation(TEXT("Join requires a completed lobby search and a session ID"));
	const int32 Index = LobbySearch->SearchResults.IndexOfByPredicate(
		[&SessionId](const FOnlineSessionSearchResult& Result) { return Result.GetSessionIdStr() == SessionId; });
	if (Index == INDEX_NONE) return FailLobbyOperation(TEXT("Selected lobby is no longer in the latest search results"));
	return JoinLobby(Index);
}

bool UHBOnlineSessionSubsystem::JoinLobby(int32 InSearchResultIndex)
{
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	int32 LocalUser;
	if (!ValidateLobbyClient(LocalUser)) return false;
	if (!LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done ||
		!LobbySearch->SearchResults.IsValidIndex(InSearchResultIndex))
		return FailLobbyOperation(TEXT("Join requires a valid result from a completed lobby search"));
	const FOnlineSessionSearchResult Result = LobbySearch->SearchResults[InSearchResultIndex];
	return JoinLobbyResult(Result, FString());
}

bool UHBOnlineSessionSubsystem::JoinLobbyResult(const FOnlineSessionSearchResult& Result, const FString& Password)
{
	if (IsLobbyOperationInProgress() || LeaveLobbyCompleteDelegateHandle.IsValid()) return false;
	int32 LocalUser;
	if (!ValidateLobbyClient(LocalUser)) return false;
	const IOnlineIdentityPtr Identity = IOnlineSubsystem::Get()->GetIdentityInterface();
	const TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUser);
	if (Result.Session.NumOpenPublicConnections <= 0) return FailLobbyOperation(TEXT("HB_ROOM_FULL"));
	if (!HBOnline::IsJoinableListenLobby(Result, UserId))
		return FailLobbyOperation(TEXT("Selected lobby is full, incompatible, self-owned, or not a HeavenBreach listen lobby"));
	FString Locked, Salt;
	Result.Session.SessionSettings.Get(FName(TEXT("HB_LOCKED")), Locked);
	Result.Session.SessionSettings.Get(FName(TEXT("HB_SALT")), Salt);
	if (Locked == TEXT("1") && (Password.IsEmpty() || Salt.IsEmpty()))
		return FailLobbyOperation(TEXT("HB_PASSWORD_REQUIRED"));
	PendingJoinProof = Locked == TEXT("1") ? HBLobbyProtocol::PasswordProof(Password, Salt) : FString();
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

	const FName SessionToDestroy = HBOnline::PartyLobbySessionName;

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
	SetLobbyOperationState(EHBLobbyOperationState::Leaving);

	const bool bRequestStarted =
		SessionInterface->DestroySession(SessionToDestroy);

	if (!bRequestStarted)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveLobbyCompleteDelegateHandle);

		LeaveLobbyCompleteDelegateHandle.Reset();
		FailLobbyOperation(TEXT("Could not leave Steam lobby"));
		bReturnAfterLeave = false;

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
	RejectLegacyMatchRequest();
}

void UHBOnlineSessionSubsystem::ConnectToPublishMatchServer()
{
	RejectLegacyMatchRequest();
}

bool UHBOnlineSessionSubsystem::RequestStartMatch()
{
	return RejectLegacyMatchRequest();
}

bool UHBOnlineSessionSubsystem::StartMatchAsLobbyOwner(const FString& ServerAddress)
{
	return RejectLegacyMatchRequest();
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
	AuxiliaryError.Reset();
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

bool UHBOnlineSessionSubsystem::RejectLegacyMatchRequest()
{
	// These reflected types/events are kept for asset compatibility only.
	// Lobby GameMode owns match start; this subsystem never publishes a server address.
	const FString Error = TEXT("Dedicated match-server APIs are retired. Start the listen match through Lobby GameMode readiness/confirmation.");
	UE_LOG(LogTemp, Warning, TEXT("Online | Legacy API | %s"), *Error);
	SetMatchStartState(EHBMatchStartState::Failed, Error);
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
	return RejectLegacyMatchRequest();
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
	if (!PendingJoinProof.IsEmpty()) ConnectString += TEXT("?HBProof=") + PendingJoinProof;
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
	PendingJoinProof.Reset();
	ConnectedLobbyMap.Reset();
	JoinFailureReason.Reset();
	LobbySearch.Reset();
	if (!bWasSuccessful) Error += TEXT("; session cleanup failed. Leave the existing session before retrying.");
	FailLobbyOperation(Error);
	if (!LobbyReturnMap.IsEmpty() && bWasSuccessful) RequestLeaveToMenu(Error);
}

void UHBOnlineSessionSubsystem::HandleLeaveLobbyComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != HBOnline::PartyLobbySessionName || !LeaveLobbyCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface.IsValid() && LeaveLobbyCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveLobbyCompleteDelegateHandle);
	}

	LeaveLobbyCompleteDelegateHandle.Reset();

	if (bWasSuccessful)
	{
		bMatchAdmissionClosed = false;
		HostedPasswordProof.Reset();
		HostedPasswordSalt.Reset();
		HostedRoomName.Reset();
		PendingJoinProof.Reset();
		ConnectedLobbyMap.Reset();
		PreTravelSettings.Reset();
		TravelSourceMap.Reset();
		TravelDestinationMap.Reset();
		SetServerTravelState(EHBServerTravelState::Idle);
		LobbySearch.Reset();
		bJoinFirstLobbyAfterFind = false;

		SetLobbyOperationState(EHBLobbyOperationState::Idle);
		PublishGamePresence(TEXT("Menu"));
		if (bReturnAfterLeave && !ReturnMenuTickerHandle.IsValid())
			ReturnMenuTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHBOnlineSessionSubsystem::ReturnToMenu));

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
		bReturnAfterLeave = false;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Online | Lobby | Leave failed: %s"),
			*InSessionName.ToString()
			);
	}

}

void UHBOnlineSessionSubsystem::HandleNetworkFailure(
	UWorld* InWorld,
	UNetDriver* InNetDriver,
	ENetworkFailure::Type InFailureType,
	const FString& InErrorString)
{
	(void)InNetDriver;
	if (bReturnAfterLeave) return;

	if (InWorld != nullptr && InWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (InWorld && ServerTravelState == EHBServerTravelState::Traveling)
	{
		FailServerTravel(FString::Printf(TEXT("Server travel network failure: %s"), *InErrorString));
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

	if (InWorld && LobbyOperationState == EHBLobbyOperationState::InLobby)
	{
		// The test flow configures a return map; legacy callers keep their own exit policy.
		FailLobbyOperation(FString::Printf(TEXT("Listen session network failure: %s"), *InErrorString));
		if (!LobbyReturnMap.IsEmpty()) RequestLeaveToMenu(TEXT("호스트 연결이 종료되었습니다."));
	}
}
