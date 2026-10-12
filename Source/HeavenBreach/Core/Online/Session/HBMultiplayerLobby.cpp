#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Core/Online/Session/HBLobbyProtocol.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
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
    if (auto* OSS = IOnlineSubsystem::Get(); OSS && OSS->GetSubsystemName() == FName(TEXT("STEAM")))
    {
        SteamPresenceInterface = OSS->GetPresenceInterface();
        if (SteamPresenceInterface.IsValid())
        {
            SteamPresenceReceivedHandle = SteamPresenceInterface->AddOnPresenceReceivedDelegate_Handle(
                FOnPresenceReceivedDelegate::CreateWeakLambda(this, [this](const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
                {
                    // 진행 중인 전체 조회는 완료 시 최신 캐시를 반영한다.
                    if (bReadingFriends) return;
                    if (const auto Friend = FindSteamFriend(UserId.ToString()); Friend.IsValid())
                        UpdateSteamFriend(*Friend, *Presence);
                }));
            SteamPresenceTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &ThisClass::TickSteamPresence), 5.f);
        }
    }
    InviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
        FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSteamInvite));
    LobbySeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelTransition.AddWeakLambda(this, [this](UWorld* World)
    {
        if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() == NM_Client
            && LobbyOperationState == EHBLobbyOperationState::InLobby)
        {
            ResetFriendSessionSearch(true);
            bMatchAdmissionClosed = true;
            if (bAutomaticInviteHubEnabled) SetInviteHubState(EHBInviteHubState::InMatch);
            PublishGamePresence(TEXT("Busy"));
        }
    });
    PublishGamePresence(TEXT("Menu"));
}

void UHBOnlineSessionSubsystem::DeinitializeMultiplayerLobby()
{
    FTSTicker::GetCoreTicker().RemoveTicker(SteamPresenceTickerHandle);
    SteamPresenceTickerHandle.Reset();
    if (SteamPresenceInterface.IsValid())
        SteamPresenceInterface->ClearOnPresenceReceivedDelegate_Handle(SteamPresenceReceivedHandle);
    SteamPresenceReceivedHandle.Reset();
    SteamPresenceInterface.Reset();
    DesiredGamePresence.Reset();
    bGamePresencePending = false;
    ++GamePresenceRequestId;
    ++FriendsReadRequestId;
    PendingSteamFriends.Reset();
    PendingFriendPresenceQueries = 0;
    ResetFriendSessionSearch(true);
    if (SessionInterface.IsValid()) SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
    FWorldDelegates::OnSeamlessTravelTransition.Remove(LobbySeamlessTravelHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(ReturnMenuTickerHandle);
    ReturnMenuTickerHandle.Reset();
    PendingInvite.Reset();
    SteamFriends.Reset();
    bReadingFriends = false;
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
    DesiredGamePresence = State;
    bGamePresencePending = true;
    GamePresenceDeadline = FPlatformTime::Seconds() + 5.;
    const uint64 RequestId = ++GamePresenceRequestId;
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
    Presence->SetPresence(*UserId, Status,
        IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
            [this, RequestId, State, Presence](const FUniqueNetId& User, bool bSuccess)
            {
                TSharedPtr<FOnlineUserPresence> Cached;
                Presence->GetCachedPresence(User, Cached);
                HandleGamePresencePublished(RequestId, State, bSuccess, Cached.Get());
            }));
}

bool UHBOnlineSessionSubsystem::HasGamePresence(const FOnlineUserPresence& Presence, const FString& State)
{
    const auto* Project = Presence.Status.Properties.Find(TEXT("HB_PROJECT"));
    const auto* CurrentState = Presence.Status.Properties.Find(TEXT("HB_STATE"));
    const auto* Joinable = Presence.Status.Properties.Find(TEXT("Joinable"));
    return Project && Project->ToString() == TEXT("HeavenBreach") && CurrentState && CurrentState->ToString() == State
        && Joinable && Joinable->ToString() == (State == TEXT("Lobby") ? TEXT("true") : TEXT("false"));
}

void UHBOnlineSessionSubsystem::HandleGamePresencePublished(uint64 RequestId, const FString& State,
    bool bSuccess, const FOnlineUserPresence* Presence)
{
    if (RequestId != GamePresenceRequestId || State != DesiredGamePresence) return;
    // Steam의 완료 콜백이 성공이어도 실제 프로젝트 키가 게시됐는지 확인한다.
    bGamePresencePending = !bSuccess || !Presence || !HasGamePresence(*Presence, State);
    UE_LOG(LogTemp, Log, TEXT("Online | Presence | State=%s Verified=%d"), *State, !bGamePresencePending);
}

bool UHBOnlineSessionSubsystem::TickSteamPresence(float)
{
    if (bReadingFriends && FPlatformTime::Seconds() >= FriendsReadDeadline)
    {
        CompleteSteamFriendsRefresh(FriendsReadRequestId, false);
        ++FriendsReadRequestId; // 타임아웃 뒤 도착하는 Steam 콜백을 무시한다.
    }
    if (DesiredGamePresence.IsEmpty() || !SteamPresenceInterface.IsValid()) return true;
    if (bGamePresencePending)
    {
        if (FPlatformTime::Seconds() >= GamePresenceDeadline) PublishGamePresence(DesiredGamePresence);
        return true;
    }
    const auto* OSS = IOnlineSubsystem::Get();
    const auto Identity = OSS && OSS->GetSubsystemName() == FName(TEXT("STEAM")) ? OSS->GetIdentityInterface() : nullptr;
    const int32 LocalUser = LocalUserIndex(GetGameInstance());
    if (!Identity.IsValid() || Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn) return true;
    const auto UserId = Identity->GetUniquePlayerId(LocalUser);
    if (!UserId.IsValid()) return true;
    const uint64 RequestId = GamePresenceRequestId;
    const FString State = DesiredGamePresence;
    const auto Presence = SteamPresenceInterface;
    Presence->QueryPresence(*UserId, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
        [this, RequestId, State, Presence](const FUniqueNetId& User, bool bSuccess)
        {
            if (RequestId != GamePresenceRequestId || State != DesiredGamePresence) return;
            TSharedPtr<FOnlineUserPresence> Cached;
            Presence->GetCachedPresence(User, Cached);
            if (!bSuccess || !Cached.IsValid() || !HasGamePresence(*Cached, State)) PublishGamePresence(State);
        }));
    return true;
}

bool UHBOnlineSessionSubsystem::ReadSteamFriendPresence(const FOnlineUserPresence& Presence, FHBSteamFriendEntry& Entry,
    FString* ExcludedReason)
{
    Entry.State.Reset();
    Entry.bCanInvite = Entry.bCanJoin = false;
    if (ExcludedReason) ExcludedReason->Reset();
    auto Exclude = [&](const TCHAR* Reason)
    {
        if (ExcludedReason) *ExcludedReason = Reason;
        return false;
    };
    const auto* Project = Presence.Status.Properties.Find(TEXT("HB_PROJECT"));
    const auto* State = Presence.Status.Properties.Find(TEXT("HB_STATE"));
    if (!Presence.bIsOnline) return Exclude(TEXT("Offline"));
    if (!Presence.bIsPlayingThisGame) return Exclude(TEXT("OtherApp"));
    if (!Project) return Exclude(TEXT("MissingProject"));
    if (Project->ToString() != TEXT("HeavenBreach")) return Exclude(TEXT("OtherProject"));
    if (!State) return Exclude(TEXT("MissingState"));
    const FString Value = State->ToString();
    if (Value != TEXT("Lobby") && Value != TEXT("Match")) return Exclude(TEXT("OutsideLobbyOrMatch"));
    Entry.State = Value;
    Entry.bCanInvite = Value == TEXT("Lobby");
    const auto* Joinable = Presence.Status.Properties.Find(TEXT("Joinable"));
    Entry.bCanJoin = Entry.bCanInvite && Joinable && Joinable->ToString() == TEXT("true") &&
        Presence.SessionId.IsValid() && Presence.SessionId->IsValid();
    return true;
}

TArray<FHBSteamFriendEntry> UHBOnlineSessionSubsystem::GetSteamFriends() const
{
    auto Entries = SteamFriends;
    const bool bCanInvite = !bSteamFriendsStale && CanInviteSteamFriends();
    const auto* Current = SessionInterface.IsValid() ? SessionInterface->GetNamedSession(LobbySession) : nullptr;
    const bool bCanJoin = !bSteamFriendsStale && CanSwitchHubSession() && LobbyOperationState == EHBLobbyOperationState::InLobby &&
        !PendingInvite.IsValid() && Current && Current->SessionInfo.IsValid();
    for (auto& Entry : Entries)
    {
        Entry.bCanInvite &= bCanInvite;
        Entry.bCanJoin &= bCanJoin;
        if (Current && Current->SessionInfo.IsValid())
        {
            const auto Friend = FindSteamFriend(Entry.UserId);
            const auto FriendSession = Friend.IsValid() ? Friend->GetPresence().SessionId : nullptr;
            if (FriendSession.IsValid() && FriendSession->ToString() == Current->GetSessionIdStr())
                Entry.bCanInvite = Entry.bCanJoin = false;
        }
    }
    return Entries;
}

TSharedPtr<FOnlineFriend> UHBOnlineSessionSubsystem::FindSteamFriend(const FString& UserId) const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    const auto Friends = OSS && OSS->GetSubsystemName() == FName(TEXT("STEAM")) ? OSS->GetFriendsInterface() : nullptr;
    TArray<TSharedRef<FOnlineFriend>> Users;
    if (UserId.IsEmpty() || !Friends.IsValid() ||
        !Friends->GetFriendsList(LocalUserIndex(GetGameInstance()), EFriendsLists::ToString(EFriendsLists::Default), Users)) return nullptr;
    for (const auto& Friend : Users)
        if (Friend->GetUserId()->ToString() == UserId &&
            Friends->IsFriend(LocalUserIndex(GetGameInstance()), *Friend->GetUserId(), EFriendsLists::ToString(EFriendsLists::Default))) return Friend;
    return nullptr;
}

bool UHBOnlineSessionSubsystem::RefreshSteamFriends()
{
    if (bReadingFriends) return false;
    AuxiliaryError.Reset();
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    const auto Friends = OSS ? OSS->GetFriendsInterface() : nullptr;
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !Friends.IsValid())
    { bSteamFriendsStale = true; AuxiliaryError = TEXT("Steam 친구 목록을 사용할 수 없습니다."); return false; }
    bReadingFriends = true;
    bFriendPresenceQueryFailed = false;
    PendingSteamFriends.Reset();
    PendingFriendPresenceQueries = 0;
    FriendsReadDeadline = FPlatformTime::Seconds() + 15.;
    const uint64 RequestId = ++FriendsReadRequestId;
    const FString List = EFriendsLists::ToString(EFriendsLists::Default);
    const bool bStarted = Friends->ReadFriendsList(LocalUserIndex(GetGameInstance()), List,
        FOnReadFriendsListComplete::CreateWeakLambda(this, [this, RequestId](int32 LocalUser, bool bSuccess, const FString& ListName, const FString&)
        {
            if (!bReadingFriends || RequestId != FriendsReadRequestId) return;
            IOnlineSubsystem* CurrentOSS = IOnlineSubsystem::Get();
            const auto CurrentFriends = CurrentOSS ? CurrentOSS->GetFriendsInterface() : nullptr;
            TArray<TSharedRef<FOnlineFriend>> Users;
            const auto Presence = CurrentOSS ? CurrentOSS->GetPresenceInterface() : nullptr;
            if (!bSuccess || !CurrentFriends.IsValid() || !Presence.IsValid() || !CurrentFriends->GetFriendsList(LocalUser, ListName, Users))
            { CompleteSteamFriendsRefresh(RequestId, false); return; }
            PendingSteamFriends = Users;
            PendingFriendPresenceQueries = Users.Num();
            if (Users.IsEmpty()) { CompleteSteamFriendsRefresh(RequestId, true); return; }
            // ReadFriendsList만 반복하면 이미 만들어진 UE Presence 캐시가 남을 수 있다.
            // Steam 친구의 QueryPresence는 Steam 클라이언트 데이터로 캐시를 즉시 갱신한다.
            for (const auto& Friend : Users)
            {
                Presence->QueryPresence(*Friend->GetUserId(), IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
                    [this, RequestId](const FUniqueNetId&, bool bSuccess)
                    { HandleSteamFriendPresenceQueried(RequestId, bSuccess); }));
            }
        }));
    if (!bStarted) CompleteSteamFriendsRefresh(RequestId, false);
    return bStarted;
}

void UHBOnlineSessionSubsystem::HandleSteamFriendPresenceQueried(uint64 RequestId, bool bSuccess)
{
    if (!bReadingFriends || RequestId != FriendsReadRequestId || PendingFriendPresenceQueries <= 0) return;
    bFriendPresenceQueryFailed |= !bSuccess;
    if (--PendingFriendPresenceQueries == 0) CompleteSteamFriendsRefresh(RequestId, !bFriendPresenceQueryFailed);
}

void UHBOnlineSessionSubsystem::CompleteSteamFriendsRefresh(uint64 RequestId, bool bSuccess)
{
    if (!bReadingFriends || RequestId != FriendsReadRequestId) return;
    bReadingFriends = false;
    bSteamFriendsStale = !bSuccess;
    if (bSuccess)
    {
        SteamFriends.Reset();
        for (const auto& Friend : PendingSteamFriends) UpdateSteamFriend(*Friend, Friend->GetPresence());
        UE_LOG(LogTemp, Verbose, TEXT("Online | Friends | Read=%d Visible=%d"), PendingSteamFriends.Num(), SteamFriends.Num());
    }
    else
    {
        AuxiliaryError = TEXT("Steam 친구 상태를 불러오지 못했습니다. 기존 목록을 유지하고 다시 시도합니다.");
        UE_LOG(LogTemp, Warning, TEXT("Online | Friends | Refresh failed; keeping %d existing entries"), SteamFriends.Num());
    }
    PendingSteamFriends.Reset();
    PendingFriendPresenceQueries = 0;
}

void UHBOnlineSessionSubsystem::UpdateSteamFriend(const FOnlineFriend& Friend, const FOnlineUserPresence& Presence)
{
    const FString UserId = Friend.GetUserId()->ToString();
    FHBSteamFriendEntry Entry;
    FString Reason;
    if (!ReadSteamFriendPresence(Presence, Entry, &Reason))
    {
        SteamFriends.RemoveAll([&](const auto& Item) { return Item.UserId == UserId; });
        UE_LOG(LogTemp, Verbose, TEXT("Online | Friends | Excluded %s: %s"), *Friend.GetDisplayName(), *Reason);
        return;
    }
    Entry.UserId = UserId;
    Entry.DisplayName = Friend.GetDisplayName();
    if (auto* Existing = SteamFriends.FindByPredicate([&](const auto& Item) { return Item.UserId == UserId; }))
        *Existing = MoveTemp(Entry);
    else SteamFriends.Add(MoveTemp(Entry));
}

bool UHBOnlineSessionSubsystem::CanInviteSteamFriends() const
{
    if (LobbyOperationState != EHBLobbyOperationState::InLobby || bMatchAdmissionClosed || IsLobbyOperationInProgress() || !IsLocalLobbyHost()) return false;
    const auto* Session = SessionInterface.IsValid() ? SessionInterface->GetNamedSession(LobbySession) : nullptr;
    return Session && Session->SessionSettings.bAllowInvites && Session->NumOpenPublicConnections > 0;
}

bool UHBOnlineSessionSubsystem::InviteSteamFriend(const FString& UserId)
{
    if (!CanInviteSteamFriends()) return false;
    const auto Friend = FindSteamFriend(UserId);
    FHBSteamFriendEntry Entry;
    if (!Friend.IsValid() || !ReadSteamFriendPresence(Friend->GetPresence(), Entry) || !Entry.bCanInvite) return false;
    const auto FriendSession = Friend->GetPresence().SessionId;
    if (FriendSession.IsValid() && FriendSession->ToString() == SessionInterface->GetNamedSession(LobbySession)->GetSessionIdStr()) return false;
    return SessionInterface->SendSessionInviteToFriend(LocalUserIndex(GetGameInstance()), LobbySession, *Friend->GetUserId());
}

bool UHBOnlineSessionSubsystem::RequestJoinSteamFriend(const FString& UserId)
{
    if (!CanSwitchHubSession() || LobbyOperationState != EHBLobbyOperationState::InLobby || PendingInvite.IsValid())
    { AuxiliaryError = TEXT("현재 로비 작업이나 게임이 끝난 뒤 친구 세션에 참가해주세요."); return false; }
    UWorld* World = GetWorld();
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    const auto Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
    const auto* Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
    const int32 LocalUser = Player ? Player->GetControllerId() : INDEX_NONE;
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || World->IsInSeamlessTravel() || !World->NextURL.IsEmpty() ||
        (World->GetNetMode() != NM_ListenServer && World->GetNetMode() != NM_Client) ||
        !OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || !SessionInterface.IsValid() ||
        OSS->GetSessionInterface() != SessionInterface || !Identity.IsValid() || LocalUser < 0 ||
        Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn)
    { AuxiliaryError = TEXT("Steam에 로그인된 로비에서만 친구 세션에 참가할 수 있습니다."); return false; }
    const auto LocalId = Identity->GetUniquePlayerId(LocalUser);
    const auto Friend = FindSteamFriend(UserId);
    FHBSteamFriendEntry Entry;
    if (!LocalId.IsValid() || !LocalId->IsValid() || !Friend.IsValid() || *Friend->GetUserId() == *LocalId ||
        !ReadSteamFriendPresence(Friend->GetPresence(), Entry) || !Entry.bCanJoin)
    { AuxiliaryError = TEXT("이 게임의 참가 가능한 로비에 있는 Steam 친구를 선택해주세요."); return false; }
    const auto* Current = SessionInterface->GetNamedSession(LobbySession);
    const FString TargetLobby = Friend->GetPresence().SessionId->ToString();
    if (!Current || !Current->SessionInfo.IsValid() || Current->GetSessionIdStr() == TargetLobby)
    { AuxiliaryError = TEXT("현재 세션이 없거나 이미 같은 로비에 참가 중입니다."); return false; }

    AuxiliaryError.Reset();
    FriendSessionLocalUser = LocalUser;
    PendingFriendId = Friend->GetUserId();
    PendingFriendLobbyId = TargetLobby;
    const uint64 RequestId = ++FriendSessionRequestId;
    FindFriendSessionCompleteHandle = SessionInterface->AddOnFindFriendSessionCompleteDelegate_Handle(LocalUser,
        FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindFriendSessionComplete, RequestId));
    if (auto* Mode = World->GetAuthGameMode<AHBLobbyGameMode>()) Mode->EvaluateCanStartMatch();
    const bool bStarted = SessionInterface->FindFriendSession(LocalUser, *PendingFriendId);
    // Steam may invoke the failure callback synchronously before returning false.
    if (!bStarted && FindFriendSessionCompleteHandle.IsValid() && RequestId == FriendSessionRequestId)
    {
        ResetFriendSessionSearch();
        AuxiliaryError = TEXT("친구 세션 조회를 시작하지 못했습니다. 현재 로비는 유지됩니다.");
    }
    return bStarted;
}

void UHBOnlineSessionSubsystem::HandleFindFriendSessionComplete(int32 LocalUser, bool bSuccess,
    const TArray<FOnlineSessionSearchResult>& Results, uint64 RequestId)
{
    if (!FindFriendSessionCompleteHandle.IsValid() || RequestId != FriendSessionRequestId || LocalUser != FriendSessionLocalUser) return;
    const auto Friend = PendingFriendId.IsValid() ? FindSteamFriend(PendingFriendId->ToString()) : nullptr;
    FHBSteamFriendEntry Entry;
    const bool bFriendStillJoinable = Friend.IsValid() && ReadSteamFriendPresence(Friend->GetPresence(), Entry) &&
        Entry.bCanJoin && Friend->GetPresence().SessionId->ToString() == PendingFriendLobbyId;
    const auto* Target = Results.FindByPredicate([this](const auto& Result) { return Result.GetSessionIdStr() == PendingFriendLobbyId; });
    ResetFriendSessionSearch();
    if (!bSuccess || !Target || !bFriendStillJoinable)
    { AuxiliaryError = TEXT("친구 로비를 찾지 못했거나 상태가 변경되었습니다. 현재 로비는 유지됩니다."); return; }
    if (LobbyOperationState != EHBLobbyOperationState::InLobby)
    { AuxiliaryError = TEXT("현재 로비 상태가 변경되어 친구 참가 요청을 취소했습니다."); return; }
    BeginHubSessionSwitch(*Target);
}

void UHBOnlineSessionSubsystem::ResetFriendSessionSearch(bool bCancelSearch)
{
    if (!FindFriendSessionCompleteHandle.IsValid()) return;
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnFindFriendSessionCompleteDelegate_Handle(FriendSessionLocalUser, FindFriendSessionCompleteHandle);
    FindFriendSessionCompleteHandle.Reset();
    FriendSessionLocalUser = INDEX_NONE;
    PendingFriendId.Reset();
    PendingFriendLobbyId.Reset();
    // Steam's async task copies delegates, so removing a handle alone cannot reject a late completion.
    ++FriendSessionRequestId;
    if (bCancelSearch && SessionInterface.IsValid()) SessionInterface->CancelFindSessions();
    if (auto* World = GetWorld())
        if (auto* Mode = World->GetAuthGameMode<AHBLobbyGameMode>()) Mode->EvaluateCanStartMatch();
}

void UHBOnlineSessionSubsystem::HandleSteamInvite(bool bSuccess, int32 LocalUser, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
    if (!bSuccess || LocalUser != LocalUserIndex(GetGameInstance()) || !UserId.IsValid() || !Result.IsValid()) return;
    FString Project;
    if (!Result.Session.SessionSettings.Get(FName(TEXT("HB_PROJECT")), Project) || Project != TEXT("HeavenBreach")) return;
    if (bAutomaticInviteHubEnabled && (bMatchAdmissionClosed || QueuedHubSession.IsValid() || FindFriendSessionCompleteHandle.IsValid()))
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
