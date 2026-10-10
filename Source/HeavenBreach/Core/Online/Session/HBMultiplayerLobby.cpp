#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Core/Online/Session/HBLobbyProtocol.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
    const FName LobbySession(TEXT("HBPartyLobbySession"));
    int32 LocalUserIndex(const UGameInstance* Instance)
    {
        const auto* Player = Instance ? Instance->GetFirstGamePlayer() : nullptr;
        return Player ? Player->GetControllerId() : 0;
    }
}

void UHBOnlineSessionSubsystem::InitializeMultiplayerLobby()
{
    InviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
        FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSteamInvite));
    LobbySeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelTransition.AddWeakLambda(this, [this](UWorld* World)
    {
        if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() == NM_Client
            && LobbyOperationState == EHBLobbyOperationState::InLobby)
        {
            bMatchAdmissionClosed = true;
            if (bAutomaticInviteHubEnabled) SetInviteHubState(EHBInviteHubState::InMatch);
            PublishGamePresence(TEXT("Busy"));
        }
    });
    PublishGamePresence(TEXT("Menu"));
}

void UHBOnlineSessionSubsystem::DeinitializeMultiplayerLobby()
{
    if (SessionInterface.IsValid()) SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
    FWorldDelegates::OnSeamlessTravelTransition.Remove(LobbySeamlessTravelHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(ReturnMenuTickerHandle);
    ReturnMenuTickerHandle.Reset();
    PendingInvite.Reset();
    SteamFriends.Reset();
}

bool UHBOnlineSessionSubsystem::RequestHostRoom(const FString& Name, const FString& Password, TSoftObjectPtr<UWorld> Map)
{
    if (IsLobbyOperationInProgress()) return false;
    if (!HBLobbyProtocol::IsValidRoomName(Name) || Password.Len() > 64)
        return FailLobbyOperation(TEXT("방 이름은 1~48자, 비밀번호는 64자 이하로 입력해주세요."));
    return RequestHostLobbyInternal(4, Map, Name, Password);
}

bool UHBOnlineSessionSubsystem::RequestJoinRoom(const FString& SessionId, const FString& Password)
{
    if (IsLobbyOperationInProgress()) return false;
    if (!LobbySearch.IsValid() || LobbySearch->SearchState != EOnlineAsyncTaskState::Done)
        return FailLobbyOperation(TEXT("방 목록을 새로고침해주세요."));
    const auto* Result = LobbySearch->SearchResults.FindByPredicate([&](const auto& Item) { return Item.GetSessionIdStr() == SessionId; });
    if (!Result) return FailLobbyOperation(TEXT("방이 존재하지 않습니다. 목록을 새로고침해주세요."));
    return JoinLobbyResult(*Result, Password);
}

FString UHBOnlineSessionSubsystem::CheckLobbyPassword(const FString& Options) const
{
    if (!HostedHubProof.IsEmpty() && UGameplayStatics::ParseOption(Options, TEXT("HBInviteProof")) != HostedHubProof)
        return TEXT("HB_INVITE_REQUIRED");
    if (!HostedPasswordProof.IsEmpty() && UGameplayStatics::ParseOption(Options, TEXT("HBProof")) != HostedPasswordProof)
        return TEXT("HB_PASSWORD_INVALID");
    return FString();
}

void UHBOnlineSessionSubsystem::PublishGamePresence(const FString& State)
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM"))) return;
    const auto Identity = OSS->GetIdentityInterface();
    const auto Presence = OSS->GetPresenceInterface();
    const int32 LocalUser = LocalUserIndex(GetGameInstance());
    if (!Identity.IsValid() || !Presence.IsValid() || Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn) return;
    const auto UserId = Identity->GetUniquePlayerId(LocalUser);
    if (!UserId.IsValid()) return;
    FOnlineUserPresenceStatus Status;
    Status.State = EOnlinePresenceState::Online;
    Status.StatusStr = State == TEXT("Match") ? TEXT("HeavenBreach 게임 중") : TEXT("HeavenBreach 접속 중");
    Status.Properties.Add(TEXT("HB_PROJECT"), FString(TEXT("HeavenBreach")));
    Status.Properties.Add(TEXT("HB_STATE"), State);
    Status.Properties.Add(TEXT("Joinable"), FString(State == TEXT("Lobby") ? TEXT("true") : TEXT("false")));
    Presence->SetPresence(*UserId, Status);
}

bool UHBOnlineSessionSubsystem::RefreshSteamFriends()
{
    if (bReadingFriends) return false;
    AuxiliaryError.Reset();
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    const auto Friends = OSS ? OSS->GetFriendsInterface() : nullptr;
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !Friends.IsValid())
    { AuxiliaryError = TEXT("Steam 친구 목록을 사용할 수 없습니다."); return false; }
    bReadingFriends = true;
    const FString List = EFriendsLists::ToString(EFriendsLists::Default);
    const bool bStarted = Friends->ReadFriendsList(LocalUserIndex(GetGameInstance()), List,
        FOnReadFriendsListComplete::CreateWeakLambda(this, [this](int32 LocalUser, bool bSuccess, const FString& ListName, const FString&)
        {
            bReadingFriends = false;
            SteamFriends.Reset();
            IOnlineSubsystem* CurrentOSS = IOnlineSubsystem::Get();
            const auto CurrentFriends = CurrentOSS ? CurrentOSS->GetFriendsInterface() : nullptr;
            TArray<TSharedRef<FOnlineFriend>> Users;
            if (!bSuccess || !CurrentFriends.IsValid() || !CurrentFriends->GetFriendsList(LocalUser, ListName, Users))
            { AuxiliaryError = TEXT("Steam 친구 목록을 불러오지 못했습니다."); return; }
            for (const auto& Friend : Users)
            {
                const auto& Presence = Friend->GetPresence();
                const auto* Project = Presence.Status.Properties.Find(TEXT("HB_PROJECT"));
                const auto* State = Presence.Status.Properties.Find(TEXT("HB_STATE"));
                if (!Presence.bIsOnline || !Presence.bIsPlayingThisGame || !Project || Project->ToString() != TEXT("HeavenBreach")) continue;
                FHBSteamFriendEntry Entry;
                Entry.UserId = Friend->GetUserId()->ToString();
                Entry.DisplayName = Friend->GetDisplayName();
                Entry.bCanInvite = State && (State->ToString() == TEXT("Menu") || State->ToString() == TEXT("Lobby"));
                SteamFriends.Add(MoveTemp(Entry));
            }
        }));
    if (!bStarted) bReadingFriends = false;
    return bStarted;
}

bool UHBOnlineSessionSubsystem::InviteSteamFriend(const FString& UserId)
{
    if (LobbyOperationState != EHBLobbyOperationState::InLobby || bMatchAdmissionClosed || IsLobbyOperationInProgress() || !IsLocalLobbyHost()) return false;
    const auto* Session = SessionInterface.IsValid() ? SessionInterface->GetNamedSession(LobbySession) : nullptr;
    if (!Session || !Session->SessionSettings.bAllowInvites || Session->NumOpenPublicConnections <= 0) return false;
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    const auto Friends = OSS ? OSS->GetFriendsInterface() : nullptr;
    if (!Friends.IsValid() || !SessionInterface.IsValid()) return false;
    TArray<TSharedRef<FOnlineFriend>> Users;
    const int32 LocalUser = LocalUserIndex(GetGameInstance());
    Friends->GetFriendsList(LocalUser, EFriendsLists::ToString(EFriendsLists::Default), Users);
    for (const auto& Friend : Users)
    {
        if (Friend->GetUserId()->ToString() != UserId) continue;
        const auto& Presence = Friend->GetPresence();
        const auto* Project = Presence.Status.Properties.Find(TEXT("HB_PROJECT"));
        const auto* State = Presence.Status.Properties.Find(TEXT("HB_STATE"));
        if (!Presence.bIsOnline || !Presence.bIsPlayingThisGame || !Project || Project->ToString() != TEXT("HeavenBreach")
            || !State || (State->ToString() != TEXT("Menu") && State->ToString() != TEXT("Lobby"))) return false;
        return SessionInterface->SendSessionInviteToFriend(LocalUser, LobbySession, *Friend->GetUserId());
    }
    return false;
}

void UHBOnlineSessionSubsystem::HandleSteamInvite(bool bSuccess, int32 LocalUser, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
    if (!bSuccess || LocalUser != LocalUserIndex(GetGameInstance()) || !UserId.IsValid() || !Result.IsValid()) return;
    FString Project;
    if (!Result.Session.SessionSettings.Get(FName(TEXT("HB_PROJECT")), Project) || Project != TEXT("HeavenBreach")) return;
    if (bAutomaticInviteHubEnabled && (bMatchAdmissionClosed || QueuedHubSession.IsValid()))
    {
        AuxiliaryError = TEXT("현재 게임이나 초대 전환이 끝난 뒤 다시 초대해주세요.");
        return;
    }
    PendingInvite = MakeShared<FOnlineSessionSearchResult>(Result);
    // Steam has already shown its acceptance UI. The hub subsystem performs
    // cleanup and joining; no in-game widget is required.
    if (bAutomaticInviteHubEnabled && !IsLobbyOperationInProgress())
        if (!AcceptPendingInvite(FString())) PendingInvite.Reset();
}

bool UHBOnlineSessionSubsystem::GetPendingInvite(FHBLobbySearchEntry& Entry) const
{
    if (!PendingInvite.IsValid()) return false;
    Entry.SessionId = PendingInvite->GetSessionIdStr();
    PendingInvite->Session.SessionSettings.Get(FName(TEXT("HB_ROOM")), Entry.RoomName);
    Entry.HostName = PendingInvite->Session.OwningUserName;
    FString Locked;
    PendingInvite->Session.SessionSettings.Get(FName(TEXT("HB_LOCKED")), Locked);
    Entry.bPasswordProtected = Locked == TEXT("1");
    return true;
}

bool UHBOnlineSessionSubsystem::AcceptPendingInvite(const FString& Password)
{
    if (!PendingInvite.IsValid()) return false;
    const auto Result = *PendingInvite;
    if (bAutomaticInviteHubEnabled)
    {
        if (!BeginHubSessionSwitch(Result)) return false;
    }
    else if (!JoinLobbyResult(Result, Password)) return false;
    PendingInvite.Reset();
    return true;
}

void UHBOnlineSessionSubsystem::DeclinePendingInvite() { PendingInvite.Reset(); }

bool UHBOnlineSessionSubsystem::RequestLeaveToMenu(const FString& Notice)
{
    FString Filename;
    if (LobbyReturnMap.IsEmpty() || !FPackageName::DoesPackageExist(LobbyReturnMap, &Filename)) return false;
    if (bReturnAfterLeave) return true;
    if (IsLobbyOperationInProgress()) return false;
    MenuNotice = Notice;
    if (bAutomaticInviteHubEnabled && InviteHubState != EHBInviteHubState::LeavingForInvite)
        SetInviteHubState(EHBInviteHubState::Returning);
    bReturnAfterLeave = true;
    if (GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer)
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (auto* PC = It->IsValid() ? Cast<AHBLobbyPlayerController>(It->Get()) : nullptr; PC && !PC->IsLocalController())
                PC->ClientLobbyClosed();
    if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(LobbySession)) LeaveLobby();
    else
    {
        SetLobbyOperationState(EHBLobbyOperationState::Leaving);
        ReturnMenuTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::ReturnToMenu));
    }
    return true;
}

bool UHBOnlineSessionSubsystem::ReturnToMenu(float)
{
    ReturnMenuTickerHandle.Reset();
    bReturnAfterLeave = false;
    HostedRoomName.Reset();
    HostedPasswordProof.Reset();
    HostedPasswordSalt.Reset();
    HostedHubProof.Reset();
    PendingHubJoinProof.Reset();
    ConnectedLobbyMap.Reset();
    bMatchAdmissionClosed = false;
    PreTravelSettings.Reset();
    TravelSourceMap.Reset();
    TravelDestinationMap.Reset();
    SetServerTravelState(EHBServerTravelState::Idle);
    SetLobbyOperationState(EHBLobbyOperationState::Idle);
    PublishGamePresence(TEXT("Menu"));
    UGameplayStatics::OpenLevel(GetGameInstance(), FName(*LobbyReturnMap), true);
    return false;
}

FString UHBOnlineSessionSubsystem::ConsumeMenuNotice()
{
    FString Notice = MoveTemp(MenuNotice);
    MenuNotice.Reset();
    return Notice;
}
