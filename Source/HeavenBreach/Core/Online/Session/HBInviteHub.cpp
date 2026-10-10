#include "Core/Online/Session/HBOnlineSessionSubsystem.h"

#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/PackageName.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

namespace HBInviteHub
{
	const FName SessionName(TEXT("HBPartyLobbySession"));
	bool IsOwnStandaloneHub(const UWorld* World, const FString& Map)
	{
		return World && World->IsGameWorld() && World->HasBegunPlay() && World->GetNetMode() == NM_Standalone &&
			World->GetAuthGameMode<AHBLobbyGameMode>() &&
			UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Map;
	}
}

void UHBOnlineSessionSubsystem::SetInviteHubState(EHBInviteHubState State, const FString& Error)
{
	if (InviteHubState == State && InviteHubError == Error) return;
	InviteHubState = State;
	InviteHubError = Error;
	if (State == EHBInviteHubState::WaitingForSteam) SteamReadyDeadline = FPlatformTime::Seconds() + 60.0;
	UE_LOG(LogTemp, Log, TEXT("Online | InviteHub | State: %d | %s"), static_cast<int32>(State), *Error);
	if (State == EHBInviteHubState::WaitingForSteam || State == EHBInviteHubState::PreparingHost ||
		State == EHBInviteHubState::LeavingForInvite || State == EHBInviteHubState::JoiningInvite ||
		State == EHBInviteHubState::Returning)
		PublishGamePresence(TEXT("Busy"));
}

void UHBOnlineSessionSubsystem::NotifyInviteHubEntered(UWorld* HubWorld)
{
	if (!HubWorld || HubWorld->GetGameInstance() != GetGameInstance() || !HubWorld->IsGameWorld() ||
		!HubWorld->GetAuthGameMode<AHBLobbyGameMode>() || HubWorld->GetNetMode() == NM_Client ||
		HubWorld->GetNetMode() == NM_DedicatedServer) return;
	const FString Map = UWorld::RemovePIEPrefix(HubWorld->GetOutermost()->GetName());
	if (!FPackageName::DoesPackageExist(Map)) return;
	if (!bAutomaticInviteHubEnabled)
	{
		// Preserve an explicitly managed session, including existing Steam smoke tests.
		if (IsLobbyOperationInProgress() || (SessionInterface.IsValid() &&
			(SessionInterface->GetNamedSession(HBInviteHub::SessionName) || SessionInterface->GetNamedSession(NAME_GameSession)))) return;
		bAutomaticInviteHubEnabled = true;
		OwnHubMap = Map;
		LobbyReturnMap = Map;
		SetInviteHubState(EHBInviteHubState::WaitingForSteam);
	}
	if (!InviteHubTickerHandle.IsValid())
		InviteHubTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &ThisClass::TickInviteHub), 0.1f);
	// Never reset Joining/Leaving here: OpenLevel creates this GameMode again.
}

bool UHBOnlineSessionSubsystem::BeginHubSessionSwitch(const FOnlineSessionSearchResult& Result)
{
	if (!bAutomaticInviteHubEnabled || OwnHubMap.IsEmpty() || IsLobbyOperationInProgress() ||
		bMatchAdmissionClosed || bReturnAfterLeave || QueuedHubSession.IsValid() ||
		(InviteHubState != EHBInviteHubState::Hosting && InviteHubState != EHBInviteHubState::Guest &&
		 InviteHubState != EHBInviteHubState::WaitingForSteam && InviteHubState != EHBInviteHubState::Suspended))
	{
		AuxiliaryError = TEXT("Cannot switch hub sessions while starting, playing, or changing sessions.");
		return false;
	}
	if (!ValidateHubSession(Result)) return false; // Validate either join source before tearing down a healthy hub.
	const EHBInviteHubState Previous = InviteHubState;
	QueuedHubSession = MakeShared<FOnlineSessionSearchResult>(Result);
	SetInviteHubState(EHBInviteHubState::LeavingForInvite);
	if (!RequestLeaveToMenu())
	{
		QueuedHubSession.Reset();
		SetInviteHubState(Previous);
		return false;
	}
	return true;
}

bool UHBOnlineSessionSubsystem::RequestReturnToOwnHub(const FString& Notice)
{
	if (!bAutomaticInviteHubEnabled || OwnHubMap.IsEmpty() || !GetWorld() ||
		!GetWorld()->IsGameWorld() || IsLobbyOperationInProgress()) return false;
	QueuedHubSession.Reset();
	PendingInvite.Reset();
	SetInviteHubState(EHBInviteHubState::Returning);
	return RequestLeaveToMenu(Notice);
}

bool UHBOnlineSessionSubsystem::RetryInviteHub()
{
	if (!bAutomaticInviteHubEnabled || InviteHubState != EHBInviteHubState::Failed || IsLobbyOperationInProgress() ||
		!HBInviteHub::IsOwnStandaloneHub(GetWorld(), OwnHubMap) ||
		(SessionInterface.IsValid() && (SessionInterface->GetNamedSession(HBInviteHub::SessionName) ||
		 SessionInterface->GetNamedSession(NAME_GameSession)))) return false;
	SetInviteHubState(EHBInviteHubState::WaitingForSteam);
	return true;
}

void UHBOnlineSessionSubsystem::NotifyInviteHubSessionReady(bool bHost)
{
	if (!bAutomaticInviteHubEnabled) return;
	QueuedHubSession.Reset();
	SetInviteHubState(bHost ? EHBInviteHubState::Hosting : EHBInviteHubState::Guest);
}

void UHBOnlineSessionSubsystem::NotifyInviteHubSessionLeft()
{
	if (!bAutomaticInviteHubEnabled) return;
	if (InviteHubState != EHBInviteHubState::LeavingForInvite && InviteHubState != EHBInviteHubState::Returning)
		SetInviteHubState(EHBInviteHubState::Suspended);
}

bool UHBOnlineSessionSubsystem::TickInviteHub(float DeltaSeconds)
{
	if (!bAutomaticInviteHubEnabled) { InviteHubTickerHandle.Reset(); return false; }
	UWorld* World = GetWorld();
	const bool bOwnHub = HBInviteHub::IsOwnStandaloneHub(World, OwnHubMap);
	const bool bHasSession = SessionInterface.IsValid() &&
		(SessionInterface->GetNamedSession(HBInviteHub::SessionName) || SessionInterface->GetNamedSession(NAME_GameSession));

	if (PendingInvite.IsValid() && !IsLobbyOperationInProgress() &&
		(InviteHubState == EHBInviteHubState::Hosting || InviteHubState == EHBInviteHubState::Guest ||
		 InviteHubState == EHBInviteHubState::WaitingForSteam))
		if (!AcceptPendingInvite(FString())) PendingInvite.Reset();

	if (InviteHubState == EHBInviteHubState::PreparingHost)
	{
		if (LobbyOperationState == EHBLobbyOperationState::Failed)
			SetInviteHubState(EHBInviteHubState::Failed, GetLastLobbyOperationError());
		return true;
	}
	if (InviteHubState == EHBInviteHubState::LeavingForInvite)
	{
		if (LobbyOperationState == EHBLobbyOperationState::Failed)
		{
			QueuedHubSession.Reset();
			SetInviteHubState(EHBInviteHubState::Failed, GetLastLobbyOperationError());
			return true;
		}
		if (!bOwnHub || bReturnAfterLeave || IsLobbyOperationInProgress() || bHasSession) return true;
		if (!QueuedHubSession.IsValid() || !ValidateHubSession(*QueuedHubSession))
		{
			const FString Error = GetLastLobbyOperationError();
			QueuedHubSession.Reset();
			MenuNotice = Error;
			SetInviteHubState(EHBInviteHubState::WaitingForSteam);
			return true;
		}
		const FOnlineSessionSearchResult Result = *QueuedHubSession;
		SetInviteHubState(EHBInviteHubState::JoiningInvite);
		if (!JoinLobbyResult(Result, FString()) && !IsLobbyOperationInProgress())
		{
			QueuedHubSession.Reset();
			RequestReturnToOwnHub(GetLastLobbyOperationError());
		}
		return true;
	}
	if (InviteHubState == EHBInviteHubState::JoiningInvite && LobbyOperationState == EHBLobbyOperationState::Failed)
	{
		QueuedHubSession.Reset();
		const FString Error = GetLastLobbyOperationError();
		if (bHasSession) SetInviteHubState(EHBInviteHubState::Failed, Error);
		else RequestReturnToOwnHub(Error);
		return true;
	}
	if (InviteHubState == EHBInviteHubState::Returning || InviteHubState == EHBInviteHubState::Suspended)
	{
		if (LobbyOperationState == EHBLobbyOperationState::Failed && bHasSession)
		{
			SetInviteHubState(EHBInviteHubState::Failed, GetLastLobbyOperationError());
			return true;
		}
		if (bOwnHub && !bReturnAfterLeave && !IsLobbyOperationInProgress() && !bHasSession)
			SetInviteHubState(EHBInviteHubState::WaitingForSteam);
	}
	if (InviteHubState != EHBInviteHubState::WaitingForSteam || !bOwnHub ||
		bReturnAfterLeave || IsLobbyOperationInProgress() || bHasSession) return true;
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
	const ULocalPlayer* Player = GetGameInstance()->GetFirstGamePlayer();
	const int32 LocalUser = Player ? Player->GetControllerId() : INDEX_NONE;
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !SessionInterface.IsValid() ||
		OSS->GetSessionInterface() != SessionInterface || !Identity.IsValid() || LocalUser < 0 ||
		Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn)
	{
		if (FPlatformTime::Seconds() > SteamReadyDeadline)
			SetInviteHubState(EHBInviteHubState::Failed, TEXT("Steam login was not available within 60 seconds; retry after logging in."));
		return true;
	}
	const FString Name = Identity->GetPlayerNickname(LocalUser);
	SetInviteHubState(EHBInviteHubState::PreparingHost);
	if (!RequestHostLobbyInternal(4, TSoftObjectPtr<UWorld>(FSoftObjectPath(OwnHubMap)),
		Name.IsEmpty() ? TEXT("HeavenBreach") : Name, FString(), true) && !IsLobbyOperationInProgress())
		SetInviteHubState(EHBInviteHubState::Failed, GetLastLobbyOperationError());
	return true;
}
