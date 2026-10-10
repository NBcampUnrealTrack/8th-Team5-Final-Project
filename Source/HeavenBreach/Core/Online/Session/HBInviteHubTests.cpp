#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h"

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
	Presence = LobbyPresence; Presence.Status.Properties.Add(TEXT("Joinable"), FString(TEXT("false")));
	TestTrue(TEXT("Lobby remains visible when its admission is unavailable"), Sessions->ReadSteamFriendPresence(Presence, Entry));
	TestFalse(TEXT("Closed presence cannot enable joining"), Entry.bCanJoin);
	Presence = LobbyPresence; Presence.SessionId.Reset();
	TestTrue(TEXT("Lobby state alone can still be displayed"), Sessions->ReadSteamFriendPresence(Presence, Entry));
	TestFalse(TEXT("Missing lobby ID cannot enable joining"), Entry.bCanJoin);

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
