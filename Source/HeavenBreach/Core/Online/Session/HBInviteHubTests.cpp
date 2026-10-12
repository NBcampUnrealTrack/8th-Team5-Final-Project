#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	// 실제 Steam 계정 없이 비동기 친구 조회의 완료 순서와 복구를 검증한다.
	class FHBTestSteamFriend final : public FOnlineFriend
	{
		FUniqueNetIdRef Id;
		FOnlineUserPresence Presence;
	public:
		FHBTestSteamFriend(const FString& UserId, const FOnlineUserPresence& InPresence)
			: Id(FUniqueNetIdString::Create(UserId, TEXT("test"))), Presence(InPresence) {}
		virtual FUniqueNetIdRef GetUserId() const override { return Id; }
		virtual FString GetRealName() const override { return TEXT("Steam friend"); }
		virtual FString GetDisplayName(const FString& = TEXT("")) const override { return TEXT("Steam friend"); }
		virtual bool GetUserAttribute(const FString&, FString&) const override { return false; }
		virtual EInviteStatus::Type GetInviteStatus() const override { return EInviteStatus::Accepted; }
		virtual const FOnlineUserPresence& GetPresence() const override { return Presence; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBInviteHubGuardsTest, "HeavenBreach.Online.InviteHubGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBInviteHubGuardsTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UHBOnlineSessionSubsystem* Sessions = NewObject<UHBOnlineSessionSubsystem>(Instance);
	TestEqual(TEXT("Automatic hosting is opt-in through a lobby GameMode"), Sessions->GetInviteHubState(), EHBInviteHubState::Disabled);
	TestFalse(TEXT("No world cannot claim Steam ownership"), Sessions->IsLocalLobbyHost());
	TestFalse(TEXT("No registered hub cannot return"), Sessions->RequestReturnToOwnHub());
	Sessions->NotifyInviteHubEntered(nullptr);
	TestFalse(TEXT("Invalid entry never enables automatic hosting"), Sessions->IsAutomaticInviteHubEnabled());

	Sessions->bAutomaticInviteHubEnabled = true;
	Sessions->OwnHubMap = TEXT("/Game/Levels/Lobby/L_Lobby");
	Sessions->LobbyReturnMap = Sessions->OwnHubMap;
	Sessions->SetLobbyReturnMap(TEXT("/Game/Levels/Test_CW/Multiplayer/L_MultiplayerTestMenu"));
	TestEqual(TEXT("Legacy UI cannot overwrite the personal hub return map"), Sessions->LobbyReturnMap, Sessions->OwnHubMap);
	TestFalse(TEXT("Automatic hubs reject room search"), Sessions->FindLobbies());
	TestEqual(TEXT("Rejected search does not disturb the session operation"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);

	Sessions->HostedHubProof = TEXT("0123456789abcdef0123456789abcdef");
	TestEqual(TEXT("Endpoint without hub admission data is rejected"), Sessions->CheckLobbyPassword(TEXT("")), FString(TEXT("HB_INVITE_REQUIRED")));
	TestEqual(TEXT("Wrong hub admission data is rejected"), Sessions->CheckLobbyPassword(TEXT("?HBInviteProof=wrong")), FString(TEXT("HB_INVITE_REQUIRED")));
	TestTrue(TEXT("Shared hub admission data is accepted"), Sessions->CheckLobbyPassword(TEXT("?HBInviteProof=0123456789abcdef0123456789abcdef")).IsEmpty());
	Sessions->HostedHubProof.Reset();
	TestTrue(TEXT("Legacy room admission remains unchanged"), Sessions->CheckLobbyPassword(TEXT("")).IsEmpty());

	FOnlineSessionSearchResult Invalid;
	for (EHBInviteHubState State : {EHBInviteHubState::PreparingHost, EHBInviteHubState::LeavingForInvite,
		EHBInviteHubState::JoiningInvite, EHBInviteHubState::InMatch, EHBInviteHubState::Returning})
	{
		Sessions->InviteHubState = State;
		TestFalse(TEXT("Session switch cannot interrupt an active hub transition"), Sessions->BeginHubSessionSwitch(Invalid));
		TestEqual(TEXT("Rejected switch preserves the active transition"), Sessions->GetInviteHubState(), State);
		TestFalse(TEXT("Rejected switch cannot queue a second join"), Sessions->QueuedHubSession.IsValid());
	}
	Sessions->InviteHubState = EHBInviteHubState::Hosting;
	Sessions->bMatchAdmissionClosed = true;
	TestFalse(TEXT("Admission closure immediately blocks switching hosts"), Sessions->BeginHubSessionSwitch(Invalid));
	Sessions->bMatchAdmissionClosed = false;
	TestFalse(TEXT("Invalid join target cannot destroy the current hub"), Sessions->BeginHubSessionSwitch(Invalid));
	TestEqual(TEXT("Preflight rejection keeps the host state"), Sessions->GetInviteHubState(), EHBInviteHubState::Hosting);

	for (EHBInviteHubState State : {EHBInviteHubState::Guest, EHBInviteHubState::InMatch, EHBInviteHubState::Suspended})
	{
		Sessions->InviteHubState = State;
		Sessions->TickInviteHub(0.f);
		TestEqual(TEXT("Guest, match and result states do not create a personal session"), Sessions->GetInviteHubState(), State);
		TestEqual(TEXT("No unintended session operation is started"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
	}
	Sessions->QueuedHubSession = MakeShared<FOnlineSessionSearchResult>();
	Sessions->InviteHubState = EHBInviteHubState::LeavingForInvite;
	Sessions->ResetLobbyAfterLeave();
	TestEqual(TEXT("Leave completion preserves the pending session switch"), Sessions->GetInviteHubState(), EHBInviteHubState::LeavingForInvite);
	TestTrue(TEXT("Leave completion retains the target session"), Sessions->QueuedHubSession.IsValid());
	Sessions->QueuedHubSession.Reset();
	Sessions->InviteHubState = EHBInviteHubState::InMatch;
	Sessions->ResetLobbyAfterLeave();
	TestEqual(TEXT("Result departure suspends hosting until hub return"), Sessions->GetInviteHubState(), EHBInviteHubState::Suspended);
	Sessions->InviteHubState = EHBInviteHubState::Failed;
	TestFalse(TEXT("Retry requires a real standalone hub world"), Sessions->RetryInviteHub());
	TestEqual(TEXT("Failure does not retry in a loop"), Sessions->GetInviteHubState(), EHBInviteHubState::Failed);
	Sessions->InviteHubState = EHBInviteHubState::JoiningInvite;
	Sessions->QueuedHubSession = MakeShared<FOnlineSessionSearchResult>();
	Sessions->LobbyOperationState = EHBLobbyOperationState::Failed;
	Sessions->TickInviteHub(0.f);
	TestFalse(TEXT("Failed join drops the queued target before recovery"), Sessions->QueuedHubSession.IsValid());
	Sessions->bHostRequestActive = true;
	Sessions->HostFailureReason = TEXT("simulated host failure");
	Sessions->LobbyOperationState = EHBLobbyOperationState::CleaningUpHost;
	AddExpectedError(TEXT("simulated host failure"), EAutomationExpectedErrorFlags::Contains, 1);
	Sessions->HandleHostCleanupComplete(FName(TEXT("HBPartyLobbySession")), true);
	TestEqual(TEXT("Host failure stops rather than automatically creating again"), Sessions->GetInviteHubState(), EHBInviteHubState::Failed);
	TestFalse(TEXT("Host failure does not schedule a map reload loop"), Sessions->bReturnAfterLeave);

	FOnlineUserPresence LobbyPresence;
	LobbyPresence.bIsOnline = LobbyPresence.bIsPlayingThisGame = true;
	LobbyPresence.SessionId = FUniqueNetIdString::Create(TEXT("remote-lobby"), TEXT("test"));
	LobbyPresence.Status.Properties.Add(TEXT("HB_PROJECT"), FString(TEXT("HeavenBreach")));
	LobbyPresence.Status.Properties.Add(TEXT("HB_STATE"), FString(TEXT("Lobby")));
	LobbyPresence.Status.Properties.Add(TEXT("Joinable"), FString(TEXT("true")));
	FHBSteamFriendEntry Entry;
	TestTrue(TEXT("Same-project lobby friend is visible"), Sessions->ReadSteamFriendPresence(LobbyPresence, Entry));
	TestEqual(TEXT("Friend state preserves the existing presence protocol"), Entry.State, FString(TEXT("Lobby")));
	TestTrue(TEXT("Joinable lobby friend can receive invitations and be a join target"), Entry.bCanInvite && Entry.bCanJoin);
	auto Presence = LobbyPresence;
	Presence.Status.Properties.Add(TEXT("HB_STATE"), FString(TEXT("Match")));
	TestTrue(TEXT("Playing friend remains visible"), Sessions->ReadSteamFriendPresence(Presence, Entry));
	TestEqual(TEXT("Playing state is available to the later UI bridge"), Entry.State, FString(TEXT("Match")));
	TestFalse(TEXT("Playing friend cannot be invited or joined"), Entry.bCanInvite || Entry.bCanJoin);
	auto Hidden = [&](const TCHAR* Reason, const FOnlineUserPresence& Candidate)
	{
		TestFalse(Reason, Sessions->ReadSteamFriendPresence(Candidate, Entry));
		TestTrue(TEXT("Hidden friend has no stale state or permissions"), Entry.State.IsEmpty() && !Entry.bCanInvite && !Entry.bCanJoin);
	};
	for (const TCHAR* State : {TEXT("Menu"), TEXT("Busy"), TEXT("Unknown")})
	{
		Presence = LobbyPresence; Presence.Status.Properties.Add(TEXT("HB_STATE"), FString(State));
		Hidden(TEXT("States outside lobby and match are hidden"), Presence);
	}
	Presence = LobbyPresence; Presence.bIsOnline = false;
	Hidden(TEXT("Offline friend is hidden"), Presence);
	Presence = LobbyPresence; Presence.bIsPlayingThisGame = false;
	Hidden(TEXT("Friend in another app is hidden"), Presence);
	Presence = LobbyPresence; Presence.Status.Properties.Remove(TEXT("HB_PROJECT"));
	Hidden(TEXT("Unidentified project is hidden"), Presence);
	Presence = LobbyPresence; Presence.Status.Properties.Add(TEXT("HB_PROJECT"), FString(TEXT("OtherGame")));
	Hidden(TEXT("Another AppID 480 project is hidden"), Presence);
	Presence = LobbyPresence; Presence.Status.Properties.Remove(TEXT("HB_STATE"));
	Hidden(TEXT("Missing friend state is hidden"), Presence);
	FString ExcludedReason;
	Sessions->ReadSteamFriendPresence(Presence, Entry, &ExcludedReason);
	TestEqual(TEXT("Missing state has a diagnostic reason"), ExcludedReason, FString(TEXT("MissingState")));
	Presence = LobbyPresence; Presence.Status.Properties.Add(TEXT("Joinable"), FString(TEXT("false")));
	TestTrue(TEXT("Lobby remains visible when its admission is unavailable"), Sessions->ReadSteamFriendPresence(Presence, Entry));
	TestFalse(TEXT("Closed presence cannot enable joining"), Entry.bCanJoin);
	Presence = LobbyPresence; Presence.SessionId.Reset();
	TestTrue(TEXT("Lobby state alone can still be displayed"), Sessions->ReadSteamFriendPresence(Presence, Entry));
	TestFalse(TEXT("Missing lobby ID cannot enable joining"), Entry.bCanJoin);

	auto* RefreshSessions = NewObject<UHBOnlineSessionSubsystem>(Instance);
	Sessions->ReadSteamFriendPresence(LobbyPresence, Entry);
	Entry.UserId = TEXT("old-friend");
	RefreshSessions->SteamFriends.Add(Entry);
	const auto VisibleFriend = MakeShared<FHBTestSteamFriend>(TEXT("fresh-friend"), LobbyPresence);
	auto OtherPresence = LobbyPresence;
	OtherPresence.Status.Properties.Add(TEXT("HB_PROJECT"), FString(TEXT("OtherGame")));
	const auto OtherFriend = MakeShared<FHBTestSteamFriend>(TEXT("other-project"), OtherPresence);
	auto QueueRefresh = [&](uint64 RequestId)
	{
		RefreshSessions->bReadingFriends = true;
		RefreshSessions->FriendsReadRequestId = RequestId;
		RefreshSessions->bFriendPresenceQueryFailed = false;
		RefreshSessions->PendingSteamFriends = {VisibleFriend, OtherFriend};
		RefreshSessions->PendingFriendPresenceQueries = 2;
	};
	QueueRefresh(40);
	RefreshSessions->HandleSteamFriendPresenceQueried(40, true);
	TestTrue(TEXT("List remains loading until all friend presence queries complete"), RefreshSessions->IsReadingFriends());
	TestEqual(TEXT("Partial completion retains the displayed list"), RefreshSessions->SteamFriends[0].UserId, FString(TEXT("old-friend")));
	AddExpectedError(TEXT("Online | Friends | Refresh failed; keeping"), EAutomationExpectedErrorFlags::Contains, 2);
	RefreshSessions->HandleSteamFriendPresenceQueried(40, false);
	TestFalse(TEXT("Presence failure releases the loading state"), RefreshSessions->IsReadingFriends());
	TestTrue(TEXT("Failed read marks cached actions unavailable"), RefreshSessions->bSteamFriendsStale);
	TestEqual(TEXT("Presence failure preserves existing friends"), RefreshSessions->SteamFriends[0].UserId, FString(TEXT("old-friend")));
	QueueRefresh(50);
	RefreshSessions->HandleSteamFriendPresenceQueried(40, true);
	TestEqual(TEXT("Older query completion cannot consume a newer request"), RefreshSessions->PendingFriendPresenceQueries, 2);
	RefreshSessions->HandleSteamFriendPresenceQueried(50, true);
	RefreshSessions->HandleSteamFriendPresenceQueried(50, true);
	TestFalse(TEXT("Successful retry clears the stale state"), RefreshSessions->bSteamFriendsStale);
	TestEqual(TEXT("Successful retry retains project filtering"), RefreshSessions->SteamFriends.Num(), 1);
	if (RefreshSessions->SteamFriends.Num() == 1)
	{
		TestEqual(TEXT("Successful retry publishes newly available friend data"), RefreshSessions->SteamFriends[0].UserId, FString(TEXT("fresh-friend")));
		TestEqual(TEXT("Successful retry preserves the Steam display name"), RefreshSessions->SteamFriends[0].DisplayName, FString(TEXT("Steam friend")));
	}
	Presence = LobbyPresence;
	Presence.Status.Properties.Add(TEXT("HB_STATE"), FString(TEXT("Match")));
	RefreshSessions->UpdateSteamFriend(*VisibleFriend, Presence);
	RefreshSessions->UpdateSteamFriend(*VisibleFriend, Presence);
	TestEqual(TEXT("Repeated presence events never duplicate friend rows"), RefreshSessions->SteamFriends.Num(), 1);
	if (RefreshSessions->SteamFriends.Num() == 1)
	{
		TestEqual(TEXT("Presence event immediately updates the displayed state"), RefreshSessions->SteamFriends[0].State, FString(TEXT("Match")));
		TestFalse(TEXT("Playing friend cannot acquire actions from its previous lobby state"), RefreshSessions->SteamFriends[0].bCanInvite || RefreshSessions->SteamFriends[0].bCanJoin);
	}
	Presence.bIsOnline = false;
	RefreshSessions->UpdateSteamFriend(*VisibleFriend, Presence);
	TestTrue(TEXT("Offline presence removes the former visible row"), RefreshSessions->SteamFriends.IsEmpty());
	RefreshSessions->UpdateSteamFriend(*VisibleFriend, LobbyPresence);
	TestEqual(TEXT("Fresh lobby presence restores a previously hidden friend"), RefreshSessions->SteamFriends.Num(), 1);
	QueueRefresh(60);
	RefreshSessions->FriendsReadDeadline = 0.;
	RefreshSessions->TickSteamPresence(0.f);
	TestFalse(TEXT("Missing callback times out instead of leaving loading locked"), RefreshSessions->IsReadingFriends());
	RefreshSessions->HandleSteamFriendPresenceQueried(60, true);
	TestEqual(TEXT("Timed-out callback cannot replace the retained list"), RefreshSessions->SteamFriends.Num(), 1);
	QueueRefresh(70);
	RefreshSessions->PendingSteamFriends.Reset();
	RefreshSessions->PendingFriendPresenceQueries = 0;
	RefreshSessions->CompleteSteamFriendsRefresh(70, true);
	TestTrue(TEXT("Successful empty read legitimately clears old friends"), RefreshSessions->SteamFriends.IsEmpty());
	TestFalse(TEXT("Successful empty read clears loading"), RefreshSessions->IsReadingFriends());

	RefreshSessions->DesiredGamePresence = TEXT("Lobby");
	RefreshSessions->GamePresenceRequestId = 80;
	RefreshSessions->bGamePresencePending = true;
	RefreshSessions->HandleGamePresencePublished(80, TEXT("Lobby"), false, &LobbyPresence);
	TestTrue(TEXT("Failed presence publication remains pending for retry"), RefreshSessions->bGamePresencePending);
	Presence = LobbyPresence; Presence.Status.Properties.Remove(TEXT("HB_PROJECT"));
	RefreshSessions->HandleGamePresencePublished(80, TEXT("Lobby"), true, &Presence);
	TestTrue(TEXT("Steam completion success alone cannot acknowledge missing project keys"), RefreshSessions->bGamePresencePending);
	RefreshSessions->HandleGamePresencePublished(80, TEXT("Lobby"), true, &LobbyPresence);
	TestFalse(TEXT("Verified local keys finish publication"), RefreshSessions->bGamePresencePending);
	RefreshSessions->DesiredGamePresence = TEXT("Match");
	RefreshSessions->GamePresenceRequestId = 81;
	RefreshSessions->bGamePresencePending = true;
	RefreshSessions->HandleGamePresencePublished(80, TEXT("Lobby"), true, &LobbyPresence);
	TestTrue(TEXT("Old lobby publication cannot finish a newer match request"), RefreshSessions->bGamePresencePending);
	Presence = LobbyPresence;
	Presence.Status.Properties.Add(TEXT("HB_STATE"), FString(TEXT("Match")));
	Presence.Status.Properties.Add(TEXT("Joinable"), FString(TEXT("false")));
	RefreshSessions->HandleGamePresencePublished(81, TEXT("Match"), true, &Presence);
	TestFalse(TEXT("Current match publication verifies its closed admission"), RefreshSessions->bGamePresencePending);
	QueueRefresh(90);
	RefreshSessions->DeinitializeMultiplayerLobby();
	RefreshSessions->HandleSteamFriendPresenceQueried(90, true);
	RefreshSessions->HandleGamePresencePublished(81, TEXT("Match"), false, nullptr);
	TestFalse(TEXT("Shutdown rejects late friend and publication callbacks"), RefreshSessions->IsReadingFriends() || RefreshSessions->bGamePresencePending);

	auto* FriendSessions = NewObject<UHBOnlineSessionSubsystem>(Instance);
	FriendSessions->bAutomaticInviteHubEnabled = true;
	FriendSessions->OwnHubMap = FriendSessions->LobbyReturnMap = TEXT("/Game/Levels/Lobby/L_Lobby");
	FriendSessions->InviteHubState = EHBInviteHubState::Hosting;
	FriendSessions->LobbyOperationState = EHBLobbyOperationState::InLobby;
	FriendSessions->HostedHubProof = TEXT("existing-host-admission");
	TestFalse(TEXT("Friend join without a live Steam lobby is rejected"), FriendSessions->RequestJoinSteamFriend(TEXT("friend")));
	TestEqual(TEXT("Failed preflight retains the current lobby"), FriendSessions->GetLobbyOperationState(), EHBLobbyOperationState::InLobby);
	FriendSessions->ReadSteamFriendPresence(LobbyPresence, Entry);
	FriendSessions->SteamFriends.Add(Entry);
	const auto Snapshot = FriendSessions->GetSteamFriends();
	TestFalse(TEXT("Snapshot cannot grant permissions without a live local session"), Snapshot[0].bCanInvite || Snapshot[0].bCanJoin);

	auto QueueFriendLookup = [&](uint64 RequestId)
	{
		FriendSessions->FindFriendSessionCompleteHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
		FriendSessions->FriendSessionLocalUser = 0;
		FriendSessions->FriendSessionRequestId = RequestId;
		FriendSessions->PendingFriendId = FUniqueNetIdString::Create(TEXT("friend"), TEXT("test"));
		FriendSessions->PendingFriendLobbyId = TEXT("remote-lobby");
	};
	QueueFriendLookup(10);
	TestTrue(TEXT("Friend lookup participates in the existing busy guard"), FriendSessions->IsLobbyOperationInProgress());
	TestEqual(TEXT("Lookup keeps the live connection's lobby state"), FriendSessions->GetLobbyOperationState(), EHBLobbyOperationState::InLobby);
	TestFalse(TEXT("Repeated friend request cannot replace the active query"), FriendSessions->RequestJoinSteamFriend(TEXT("other")));
	TestFalse(TEXT("Match travel cannot race a friend lookup"), FriendSessions->RequestServerMatchTravel(nullptr));
	TestFalse(TEXT("Invitation switching cannot race a friend lookup"), FriendSessions->BeginHubSessionSwitch(Invalid));
	FriendSessions->HandleFindFriendSessionComplete(1, false, {}, 10);
	TestTrue(TEXT("Another local user's completion does not consume the query"), FriendSessions->IsLobbyOperationInProgress());
	FriendSessions->HandleFindFriendSessionComplete(0, false, {}, 9);
	TestTrue(TEXT("Older completion does not consume the query"), FriendSessions->IsLobbyOperationInProgress());
	FriendSessions->HandleFindFriendSessionComplete(0, false, {}, 10);
	TestFalse(TEXT("Lookup failure releases the busy guard"), FriendSessions->IsLobbyOperationInProgress());
	TestEqual(TEXT("Lookup failure retains the host lifecycle"), FriendSessions->GetInviteHubState(), EHBInviteHubState::Hosting);
	TestEqual(TEXT("Lookup failure retains server admission data"), FriendSessions->HostedHubProof, FString(TEXT("existing-host-admission")));
	TestFalse(TEXT("Lookup failure never queues travel or destroys the lobby"), FriendSessions->bReturnAfterLeave || FriendSessions->QueuedHubSession.IsValid());
	TestFalse(TEXT("Completed lookup releases its friend target"), FriendSessions->PendingFriendId.IsValid());
	TestTrue(TEXT("Completed lookup releases its lobby target"), FriendSessions->PendingFriendLobbyId.IsEmpty());
	QueueFriendLookup(20);
	FriendSessions->HandleFindFriendSessionComplete(0, true, {}, 10);
	TestTrue(TEXT("Late Steam delegate copy cannot consume a newer query"), FriendSessions->IsLobbyOperationInProgress());
	FriendSessions->ResetFriendSessionSearch(true);
	const FString Notice = FriendSessions->GetLastLobbyOperationError();
	FriendSessions->HandleFindFriendSessionComplete(0, true, {}, 20);
	TestFalse(TEXT("Cancelled query cannot become busy again"), FriendSessions->IsLobbyOperationInProgress());
	TestEqual(TEXT("Cancelled completion does not overwrite the existing notice"), FriendSessions->GetLastLobbyOperationError(), Notice);
	TestEqual(TEXT("Cancellation leaves the connection policy intact"), FriendSessions->GetLobbyOperationState(), EHBLobbyOperationState::InLobby);
	return true;
}
#endif
