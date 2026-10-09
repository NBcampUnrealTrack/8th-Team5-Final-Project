#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "OnlineSessionSettings.h"

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

	Sessions->HostedInviteProof = TEXT("0123456789abcdef0123456789abcdef");
	TestEqual(TEXT("Endpoint without invite proof is rejected"), Sessions->CheckLobbyPassword(TEXT("")), FString(TEXT("HB_INVITE_REQUIRED")));
	TestEqual(TEXT("Wrong invite proof is rejected"), Sessions->CheckLobbyPassword(TEXT("?HBInviteProof=wrong")), FString(TEXT("HB_INVITE_REQUIRED")));
	TestTrue(TEXT("Private lobby admission proof is accepted"), Sessions->CheckLobbyPassword(TEXT("?HBInviteProof=0123456789abcdef0123456789abcdef")).IsEmpty());
	Sessions->HostedInviteProof.Reset();
	TestTrue(TEXT("Legacy room admission remains unchanged"), Sessions->CheckLobbyPassword(TEXT("")).IsEmpty());

	FOnlineSessionSearchResult Invalid;
	for (EHBInviteHubState State : {EHBInviteHubState::PreparingHost, EHBInviteHubState::LeavingForInvite,
		EHBInviteHubState::JoiningInvite, EHBInviteHubState::InMatch, EHBInviteHubState::Returning})
	{
		Sessions->InviteHubState = State;
		TestFalse(TEXT("Invite cannot interrupt an active hub transition"), Sessions->BeginHubInvite(Invalid, FString()));
		TestEqual(TEXT("Rejected invite preserves the active transition"), Sessions->GetInviteHubState(), State);
		TestFalse(TEXT("Rejected invite cannot queue a second join"), Sessions->QueuedHubInvite.IsValid());
	}
	Sessions->InviteHubState = EHBInviteHubState::Hosting;
	Sessions->bMatchAdmissionClosed = true;
	TestFalse(TEXT("Admission closure immediately blocks switching hosts"), Sessions->BeginHubInvite(Invalid, FString()));
	Sessions->bMatchAdmissionClosed = false;
	TestFalse(TEXT("Invalid invitation cannot destroy the current hub"), Sessions->BeginHubInvite(Invalid, FString()));
	TestEqual(TEXT("Preflight rejection keeps the host state"), Sessions->GetInviteHubState(), EHBInviteHubState::Hosting);

	for (EHBInviteHubState State : {EHBInviteHubState::Guest, EHBInviteHubState::InMatch, EHBInviteHubState::Suspended})
	{
		Sessions->InviteHubState = State;
		Sessions->TickInviteHub(0.f);
		TestEqual(TEXT("Guest, match and result states do not create a personal session"), Sessions->GetInviteHubState(), State);
		TestEqual(TEXT("No unintended session operation is started"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
	}
	Sessions->QueuedHubInvite = MakeShared<FOnlineSessionSearchResult>();
	Sessions->InviteHubState = EHBInviteHubState::LeavingForInvite;
	Sessions->ResetLobbyAfterLeave();
	TestEqual(TEXT("Leave completion preserves the pending invite transition"), Sessions->GetInviteHubState(), EHBInviteHubState::LeavingForInvite);
	TestTrue(TEXT("Leave completion retains the target invitation"), Sessions->QueuedHubInvite.IsValid());
	Sessions->QueuedHubInvite.Reset();
	Sessions->InviteHubState = EHBInviteHubState::InMatch;
	Sessions->ResetLobbyAfterLeave();
	TestEqual(TEXT("Result departure suspends hosting until hub return"), Sessions->GetInviteHubState(), EHBInviteHubState::Suspended);
	Sessions->InviteHubState = EHBInviteHubState::Failed;
	TestFalse(TEXT("Retry requires a real standalone hub world"), Sessions->RetryInviteHub());
	TestEqual(TEXT("Failure does not retry in a loop"), Sessions->GetInviteHubState(), EHBInviteHubState::Failed);
	Sessions->InviteHubState = EHBInviteHubState::JoiningInvite;
	Sessions->QueuedHubInvite = MakeShared<FOnlineSessionSearchResult>();
	Sessions->LobbyOperationState = EHBLobbyOperationState::Failed;
	Sessions->TickInviteHub(0.f);
	TestFalse(TEXT("Failed join drops the queued target before recovery"), Sessions->QueuedHubInvite.IsValid());
	Sessions->bHostRequestActive = true;
	Sessions->HostFailureReason = TEXT("simulated host failure");
	Sessions->LobbyOperationState = EHBLobbyOperationState::CleaningUpHost;
	AddExpectedError(TEXT("simulated host failure"), EAutomationExpectedErrorFlags::Contains, 1);
	Sessions->HandleHostCleanupComplete(FName(TEXT("HBPartyLobbySession")), true);
	TestEqual(TEXT("Host failure stops rather than automatically creating again"), Sessions->GetInviteHubState(), EHBInviteHubState::Failed);
	TestFalse(TEXT("Host failure does not schedule a map reload loop"), Sessions->bReturnAfterLeave);
	return true;
}
#endif
