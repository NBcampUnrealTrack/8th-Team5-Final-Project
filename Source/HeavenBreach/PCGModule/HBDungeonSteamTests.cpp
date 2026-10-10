#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Core/GameFlow/HBMatchResultSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Friends/HB_FriendsPanel.h"
#include "UI/Friends/HB_FriendEntryWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "AI/EnemyCharacterBase.h"
#include "Loot/BoxActor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
    const FName SteamTestSession(TEXT("HBPartyLobbySession"));
    const TCHAR* LobbyMap = TEXT("/Game/Levels/Lobby/L_Lobby.L_Lobby");
    const TCHAR* DungeonMap = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay.Test_MGGamePlay");
    const TCHAR* SteamDriverClass = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");

    bool VerifyFriendRow(FAutomationTestBase* Test, UHB_FriendsPanel* Panel,
        UHBOnlineSessionSubsystem* Sessions, const FString& LocalId)
    {
        FHBFriendListItem Item;
        // Use only this test account: no invite or join can reach another user.
        Item.UserId = LocalId;
        Item.DisplayName = FText::FromString(TEXT("UI 테스트 사용자"));
        Item.Status = EHBFriendStatus::InLobby;
        Item.bCanInvite = Item.bCanJoin = true;
        Panel->SetFriends({Item});
        auto* Rows = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("FriendListContainer")));
        auto* Entry = Rows && Rows->GetChildrenCount() == 1 ? Cast<UHB_FriendEntryWidget>(Rows->GetChildAt(0)) : nullptr;
        auto* Name = Entry ? Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Text_Name"))) : nullptr;
        auto* Status = Entry ? Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Text_Status"))) : nullptr;
        auto* Invite = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("Button_Invite"))) : nullptr;
        auto* Join = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("Button_Join"))) : nullptr;
        if (!Name || !Status || !Invite || !Join)
        { Test->AddError(TEXT("Production WBP_FriendEntry is missing its name, status or action buttons.")); return false; }
        Test->TestEqual(TEXT("Production friend row displays the supplied nickname"), Name->GetText().ToString(), Item.DisplayName.ToString());
        Test->TestEqual(TEXT("Lobby friend row uses the requested lobby label"), Status->GetText().ToString(), FString(TEXT("로비")));
        Test->TestTrue(TEXT("Lobby friend actions use the supplied availability"), Invite->GetIsEnabled() && Join->GetIsEnabled());
        Test->TestTrue(TEXT("Friend row forwards invitations to the existing panel"), Entry->OnInviteClicked.Contains(Panel, FName(TEXT("HandleEntryInvite"))));
        Test->TestTrue(TEXT("Panel forwards invitations to the local controller"), Panel->OnInviteRequested.Contains(Panel->GetOwningPlayer(), FName(TEXT("HandleFriendInvite"))));
        // The real Session subsystem must safely reject this synthetic self join.
        Join->OnClicked.Broadcast();
        Test->TestEqual(TEXT("Friend join click reaches the existing Steam preflight"), Sessions->GetLastLobbyOperationError(),
            FString(TEXT("이 게임의 참가 가능한 로비에 있는 Steam 친구를 선택해주세요.")));
        Test->TestEqual(TEXT("Rejected UI join keeps the current lobby"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::InLobby);
        Test->TestFalse(TEXT("Rejected UI join releases the operation lock"), Sessions->IsLobbyOperationInProgress());
        Item.Status = EHBFriendStatus::InMatch;
        Item.bCanInvite = Item.bCanJoin = false;
        Entry->SetItem(Item);
        Test->TestEqual(TEXT("Playing friend row uses the requested match label"), Status->GetText().ToString(), FString(TEXT("게임 중")));
        Test->TestFalse(TEXT("Playing friend disables both actions"), Invite->GetIsEnabled() || Join->GetIsEnabled());
        return true;
    }

    struct FHBSteamHostFixture
    {
        TWeakObjectPtr<UGameInstance> Instance;
        TWeakObjectPtr<UNetDriver> LobbyDriver;
        TArray<TWeakObjectPtr<AActor>> EndingActors;
        FHBStartingLoadout LobbyLoadout;
        FString SessionId;
        bool bAutomaticHub = false;
        bool bFriendsReadRequested = false;
		bool bLobbyUI = false;
		TWeakObjectPtr<UHBLobbyHUDWidget> LobbyHUD;
        // Isolate session/results lifecycle when dungeon authoring is unavailable.
        // The normal mode still verifies PCG, enemies and chests without this flag.
        bool bSessionOnly = false;
        FString RestoredHubId;
        double HubStableSince = 0.;
        EHBPostMatchReason EndReason = EHBPostMatchReason::None;
        EHBPostMatchReason ObservedResultReason = EHBPostMatchReason::None;
        FDelegateHandle ResultDepartureHandle;
        FDelegateHandle GameplayInitializationHandle;
    };

    // Explicitly opted-in, standalone -game test. It creates one real Steam
    // lobby, not a mock/IP session. A second Steam identity is still needed to
    // verify search, joining and replication across a real client connection.
    class FHBSteamHostTravel : public IAutomationLatentCommand
    {
        enum class EPhase { Host, Travel, Prepare, Match, Result, ReturnHub };
        FAutomationTestBase* Test;
        TSharedRef<FHBSteamHostFixture> Fixture;
        EPhase Phase = EPhase::Host;
        double Started = FPlatformTime::Seconds();

        bool Wait(const FString& Reason)
        {
            if (FPlatformTime::Seconds() - Started < 150.) return false;
            Test->AddError(Reason);
            return true;
        }

        void Next(EPhase InPhase)
        {
            Phase = InPhase;
            Started = FPlatformTime::Seconds();
        }

    public:
        FHBSteamHostTravel(FAutomationTestBase* InTest, TSharedRef<FHBSteamHostFixture> InFixture)
            : Test(InTest), Fixture(InFixture) {}

        virtual bool Update() override
        {
            UGameInstance* Instance = Fixture->Instance.Get();
            UWorld* World = Instance ? Instance->GetWorld() : nullptr;
            auto* Sessions = Instance ? Instance->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
            IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
            const IOnlineSessionPtr Online = OSS ? OSS->GetSessionInterface() : nullptr;
            if (!World || !Sessions || !Online.IsValid())
                return Wait(TEXT("Steam host lost its world or session interface."));
            if (Sessions->GetLobbyOperationState() == EHBLobbyOperationState::Failed)
            {
                Test->AddError(Sessions->GetLastLobbyOperationError());
                return true;
            }

            if (Phase == EPhase::ReturnHub)
            {
                auto* Session = Online->GetNamedSession(SteamTestSession);
                if (Sessions->GetInviteHubState() != EHBInviteHubState::Hosting || !Session ||
                    World->GetNetMode() != NM_ListenServer || Sessions->IsLobbyOperationInProgress())
                    return Wait(TEXT("Personal Steam hub did not recover after leaving gameplay/results."));
                if (Fixture->HubStableSince == 0.)
                {
                    Fixture->HubStableSince = FPlatformTime::Seconds();
                    Fixture->RestoredHubId = Session->GetSessionIdStr();
                    Test->TestNotEqual(TEXT("Recovery creates a fresh personal Steam lobby"), Fixture->RestoredHubId, Fixture->SessionId);
                }
                if (FPlatformTime::Seconds() - Fixture->HubStableSince < 3.) return false;
                Test->TestEqual(TEXT("Automatic hosting does not repeatedly recreate the hub"), Session->GetSessionIdStr(), Fixture->RestoredHubId);
                Test->TestEqual(TEXT("Recovery returns to the player's original hub map"), World->GetOutermost()->GetName(), Sessions->GetOwnHubMap());
                Test->TestTrue(TEXT("Recovered hub allows friends and invitations for exactly four players"),
                    Session->SessionSettings.NumPublicConnections == 4 && Session->SessionSettings.NumPrivateConnections == 0 &&
                    Session->SessionSettings.bShouldAdvertise && Session->SessionSettings.bAllowJoinViaPresence &&
                    Session->SessionSettings.bAllowJoinViaPresenceFriendsOnly && Session->SessionSettings.bAllowInvites);
                Test->TestTrue(TEXT("Recovered local host is authorized"), Sessions->IsLocalLobbyHost());
                Test->TestFalse(TEXT("Recovery releases match admission"), Sessions->IsMatchAdmissionClosed());
                Test->AddInfo(TEXT("Automatic friends-only Steam hub restored and stable for three seconds; remote friend/invitation joining remains unverified."));
                return true;
            }

            if (Phase == EPhase::Result)
            {
                if (World->GetOutermost()->GetName() != TEXT("/Game/Levels/ExtractionResult") ||
                    !World->HasBegunPlay() || Sessions->IsLobbyOperationInProgress())
                    return Wait(TEXT("Post-match travel or automatic Steam cleanup did not complete."));
                Test->TestEqual(TEXT("Existing result map loads as standalone"), World->GetNetMode(), NM_Standalone);
                Test->TestNotNull(TEXT("Result map creates its local player without an admission fatal"), World->GetFirstPlayerController());
                const EHBPostMatchReason ExpectedReason = Fixture->EndReason == EHBPostMatchReason::MatchEnded
                    ? EHBPostMatchReason::Died : Fixture->EndReason;
                Test->TestEqual(TEXT("Result travel preserves death/extraction and timeout forfeiture"), Fixture->ObservedResultReason, ExpectedReason);
                Test->TestNull(TEXT("Result travel automatically removes this player's Steam session"), Online->GetNamedSession(SteamTestSession));
                Test->TestEqual(TEXT("Result travel resets the lobby operation"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
                Test->TestEqual(TEXT("Result travel resets the server travel operation"), Sessions->GetServerTravelState(), EHBServerTravelState::Idle);
                Test->TestFalse(TEXT("Result travel clears persistent admission state"), Sessions->IsMatchAdmissionClosed());
                Test->TestTrue(TEXT("Result travel releases the old Steam listen driver"), !Fixture->LobbyDriver.IsValid());
                const FName ExpectedWidget(ExpectedReason == EHBPostMatchReason::Died ? TEXT("WBP_DeathResult_C") : TEXT("WBP_ExtractionResult_C"));
                bool bResultWidgetVisible = false;
                for (TObjectIterator<UUserWidget> It; It; ++It)
                    if (It->GetWorld() == World && It->IsInViewport() && It->GetClass()->GetFName() == ExpectedWidget)
                        bResultWidgetVisible = true;
                Test->TestTrue(TEXT("Existing result UI for the end reason is displayed"), bResultWidgetVisible);
                for (const auto& Actor : Fixture->EndingActors)
                    Test->TestTrue(TEXT("Result travel ends the generated gameplay actors"), !Actor.IsValid() || !Actor->HasActorBegunPlay());
                Test->AddInfo(FString::Printf(TEXT("Existing ExtractionResult travel verified with automatic Steam cleanup; reason=%d."), static_cast<int32>(Fixture->EndReason)));
                if (Fixture->bAutomaticHub)
                {
                    Test->TestEqual(TEXT("Results suspend personal hosting"), Sessions->GetInviteHubState(), EHBInviteHubState::Suspended);
                    if (!Sessions->RequestReturnToOwnHub())
                    { Test->AddError(TEXT("Results could not request personal hub return.")); return true; }
                    Next(EPhase::ReturnHub);
                    return false;
                }
                return true;
            }

            auto* Mode = World->GetAuthGameMode<AHeavenBreachGameMode>();
            if (Mode)
                FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))
                    ->SetPropertyValue_InContainer(Mode, false);

            if (Phase == EPhase::Host)
            {
                if (Sessions->GetLobbyOperationState() != EHBLobbyOperationState::InLobby)
                    return Wait(TEXT("Real Steam lobby did not become a listen host."));
                auto* Lobby = World->GetAuthGameMode<AHBLobbyGameMode>();
                FNamedOnlineSession* Session = Online->GetNamedSession(SteamTestSession);
                UNetDriver* Driver = World->GetNetDriver();
                auto* Player = World->GetFirstPlayerController();
                auto* State = Player ? Player->GetPlayerState<AHB_PlayerState>() : nullptr;
                FText LoadoutError;
                if (!Lobby || !Session || !Driver || !State || World->GetNetMode() != NM_ListenServer
                    || Driver->GetClass()->GetPathName() != SteamDriverClass
                    || !ValidateStartingLoadout(State->GetStartingLoadout(), LoadoutError))
                {
                    Test->AddError(FString::Printf(TEXT("Steam lobby must have its listen driver, Lobby GameMode and valid host loadout: %s"), *LoadoutError.ToString()));
                    return true;
                }
                const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
                const ULocalPlayer* LocalPlayer = Instance->GetFirstGamePlayer();
                const auto LocalId = Identity.IsValid() && LocalPlayer
                    ? Identity->GetUniquePlayerId(LocalPlayer->GetControllerId()) : nullptr;
				UHBLobbyWidget* LobbyUI = nullptr;
				UButton* StartButton = nullptr;
				UHB_FriendsPanel* FriendsPanel = nullptr;
				if (Fixture->bLobbyUI)
				{
					int32 HUDCount = 0;
					for (TObjectIterator<UHBLobbyHUDWidget> It; It; ++It)
						if (It->GetWorld() == World && It->GetOwningPlayer() == Player && It->IsInViewport())
						{ ++HUDCount; Fixture->LobbyHUD = *It; }
					Test->TestEqual(TEXT("Lobby has exactly one local HUD"), HUDCount, 1);
					auto* HUD = Fixture->LobbyHUD.Get();
					LobbyUI = HUD ? Cast<UHBLobbyWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::Lobby)) : nullptr;
					auto* StartModule = LobbyUI ? Cast<UUserWidget>(LobbyUI->GetWidgetFromName(TEXT("StartMatchButtonModule"))) : nullptr;
					StartButton = StartModule ? Cast<UButton>(StartModule->GetWidgetFromName(TEXT("Button_Action"))) : nullptr;
					FriendsPanel = LobbyUI ? LobbyUI->GetFriendsPanel() : nullptr;
					if (!LobbyUI || !StartButton || !FriendsPanel)
					{ Test->AddError(TEXT("Production lobby HUD is missing its lobby, start button or friends panel.")); return true; }
					Test->TestTrue(TEXT("Production lobby actions are connected"), LobbyUI->OnStartRequested.IsBound() && LobbyUI->OnReadyRequested.IsBound());
					Test->TestTrue(TEXT("Production friend actions are connected"), FriendsPanel->OnRefreshRequested.IsBound() && FriendsPanel->OnInviteRequested.IsBound() && FriendsPanel->OnJoinRequested.IsBound());
				}
                if (Fixture->bAutomaticHub)
                {
                    if (!Fixture->bFriendsReadRequested)
                    {
                        Fixture->bFriendsReadRequested = true;
						if (Fixture->bLobbyUI) FriendsPanel->Open();
                        else if (!Sessions->RefreshSteamFriends())
                        { Test->AddError(TEXT("Real Steam friend list read could not start.")); return true; }
                        return false;
                    }
                    if (Sessions->IsReadingFriends()) return Wait(TEXT("Real Steam friend list read did not finish."));
					if (Fixture->bLobbyUI)
					{
						auto* Loading = FriendsPanel->GetWidgetFromName(TEXT("LoadingIndicator"));
						if (Loading && Loading->GetVisibility() != ESlateVisibility::Collapsed)
							return Wait(TEXT("Lobby UI did not release loading after the Steam friends read."));
						if (auto* Rows = Cast<UPanelWidget>(FriendsPanel->GetWidgetFromName(TEXT("FriendListContainer"))))
						{
							const auto Friends = Sessions->GetSteamFriends();
							Test->TestEqual(TEXT("Production panel receives the real Steam friend list"), Rows->GetChildrenCount(), Friends.Num());
							for (int32 Index = 0; Index < Rows->GetChildrenCount(); ++Index)
							{
								auto* Entry = Cast<UHB_FriendEntryWidget>(Rows->GetChildAt(Index));
								if (!Entry) { Test->AddError(TEXT("Friend row has the wrong native parent.")); return true; }
								const auto* Friend = Friends.FindByPredicate([Entry](const auto& Value) { return Value.UserId == Entry->GetItem().UserId; });
								if (!Friend) { Test->AddError(TEXT("Friend row does not correspond to Steam data.")); return true; }
								Test->TestEqual(TEXT("Friend row preserves the Steam nickname"), Entry->GetItem().DisplayName.ToString(), Friend->DisplayName);
								Test->TestEqual(TEXT("Friend row preserves join availability"), Entry->GetItem().bCanJoin, Friend->bCanJoin);
								Test->TestEqual(TEXT("Friend row preserves invitation availability"), Entry->GetItem().bCanInvite, Friend->bCanInvite);
							}
						}
						else { Test->AddError(TEXT("Production friend panel has no list container.")); return true; }
					}
                    Test->TestTrue(TEXT("Real Steam friend list read completed without error"), Sessions->GetLastLobbyOperationError().IsEmpty());
                    const auto Friends = Sessions->GetSteamFriends();
                    for (const auto& Friend : Friends)
                    {
                        Test->TestTrue(TEXT("Live friend list includes only lobby and match states"), Friend.State == TEXT("Lobby") || Friend.State == TEXT("Match"));
                        if (Friend.State == TEXT("Match"))
                            Test->TestFalse(TEXT("Live playing friend cannot be invited or joined"), Friend.bCanInvite || Friend.bCanJoin);
                    }
                    Test->AddInfo(FString::Printf(TEXT("Real Steam friend list verified; visible friends=%d; remote joining remains unverified."), Friends.Num()));
					if (Fixture->bLobbyUI)
					{
						if (!LocalId.IsValid() || !VerifyFriendRow(Test, FriendsPanel, Sessions, LocalId->ToString())) return true;
						FriendsPanel->Close();
					}
                }
                Test->TestTrue(TEXT("Steam session belongs to the logged-in host"),
                    LocalId.IsValid() && Session->OwningUserId.IsValid() && *LocalId == *Session->OwningUserId);
                Test->TestTrue(TEXT("Host uses a non-LAN, non-dedicated presence lobby"),
                    !Session->SessionSettings.bIsLANMatch && !Session->SessionSettings.bIsDedicated
                    && Session->SessionSettings.bUsesPresence && Session->SessionSettings.bUseLobbiesIfAvailable);
                if (Fixture->bAutomaticHub)
                {
                    Test->TestEqual(TEXT("Hub creates itself without a room creation request"), Sessions->GetInviteHubState(), EHBInviteHubState::Hosting);
                    Test->TestEqual(TEXT("Automatic hub includes the host in its four slots"), Session->SessionSettings.NumPublicConnections, 4);
                    Test->TestTrue(TEXT("Automatic hub allows friends and invitations"), Session->SessionSettings.bShouldAdvertise &&
                        Session->SessionSettings.bAllowJoinViaPresence && Session->SessionSettings.bAllowJoinViaPresenceFriendsOnly &&
                        Session->SessionSettings.bAllowInvites && Session->SessionSettings.NumPrivateConnections == 0);
                    Test->TestEqual(TEXT("Direct endpoint cannot bypass hub admission"), Sessions->CheckLobbyPassword(TEXT("")), FString(TEXT("HB_INVITE_REQUIRED")));
                    FString Proof;
                    Session->SessionSettings.Get(FName(TEXT("HB_INVITE_PROOF")), Proof);
                    Test->TestTrue(TEXT("Friends-only lobby supplies shared admission data"), Sessions->CheckLobbyPassword(TEXT("?HBInviteProof=") + Proof).IsEmpty());
                    const auto* LobbyState = World->GetGameState<AHBLobbyGameState>();
                    Test->TestTrue(TEXT("Solo hub is ready for host start"), LobbyState && LobbyState->CanStartMatch());
                    const FString OwnLobbyId = Session->GetSessionIdStr();
                    Test->TestFalse(TEXT("Friend join API rejects the local host"), Sessions->RequestJoinSteamFriend(LocalId->ToString()));
                    Test->TestEqual(TEXT("Rejected self join keeps the original Steam lobby"), Session->GetSessionIdStr(), OwnLobbyId);
                    Test->TestFalse(TEXT("Rejected self join does not leave a lookup in progress"), Sessions->IsLobbyOperationInProgress());
                    FOnlineSessionSearchResult OwnInvite;
                    OwnInvite.Session = *Session;
                    Online->TriggerOnSessionUserInviteAcceptedDelegates(true, LocalPlayer->GetControllerId(), LocalId, OwnInvite);
                    FHBLobbySearchEntry PendingEntry;
                    Test->TestFalse(TEXT("Self invitation is consumed without a UI prompt"), Sessions->GetPendingInvite(PendingEntry));
                    Test->TestEqual(TEXT("Rejected self invitation keeps the healthy hub"), Sessions->GetInviteHubState(), EHBInviteHubState::Hosting);
                }
                Fixture->LobbyDriver = Driver;
                Fixture->LobbyLoadout = State->GetStartingLoadout();
                Fixture->SessionId = Session->GetSessionIdStr();
                Test->TestFalse(TEXT("Steam assigned a real lobby ID"), Fixture->SessionId.IsEmpty());
                // UE 5.7 Steam OSS populates OwningUserId/HostingPlayerNum but
                // leaves bHosting false. Do not manufacture that flag here:
                // the public travel API must accept an actual Steam host.
                Test->AddInfo(FString::Printf(TEXT("Real Steam lobby and SteamSockets listen driver verified; bHosting=%d; start path=%s."),
                    Session->bHosting, Fixture->bLobbyUI ? TEXT("production lobby button") : TEXT("internal match travel")));
                const TCHAR* Destination = Fixture->bSessionOnly ? TEXT("/Engine/Maps/Entry.Entry") : DungeonMap;
				if (Fixture->bLobbyUI)
				{
					auto* LobbyState = World->GetGameState<AHBLobbyGameState>();
					auto* HostState = Player->GetPlayerState<AHBLobbyPlayerState>();
					auto* ReadyModule = Cast<UUserWidget>(LobbyUI->GetWidgetFromName(TEXT("ReadyButtonModule")));
					auto* ReadyButton = ReadyModule ? Cast<UButton>(ReadyModule->GetWidgetFromName(TEXT("Button_Action"))) : nullptr;
					if (!LobbyState || !HostState || !ReadyButton)
					{ Test->AddError(TEXT("Production lobby is missing ready state/button.")); return true; }
					LobbyUI->RefreshLobbyActions();
					Test->TestTrue(TEXT("Solo Steam host can use the start button"), StartButton->GetIsEnabled());
					Test->TestFalse(TEXT("Host cannot use the guest ready button"), ReadyButton->GetIsEnabled());
					const bool bHostReady = HostState->IsReady();
					ReadyButton->OnClicked.Broadcast();
					Test->TestEqual(TEXT("Disabled ready click does not change host readiness"), HostState->IsReady(), bHostReady);
					LobbyState->SetCanStartMatch(false);
					StartButton->OnClicked.Broadcast();
					Test->TestFalse(TEXT("Unready start click does not close admission"), Sessions->IsMatchAdmissionClosed());
					Test->TestFalse(TEXT("Start button follows replicated readiness"), StartButton->GetIsEnabled());
					LobbyState->SetCanStartMatch(true);
					LobbyUI->RefreshLobbyActions();
					// Exercise the same property used by the production Blueprint; never save the fixture's map.
					auto* MapProperty = FindFProperty<FSoftObjectProperty>(Lobby->GetClass(), TEXT("GameplayMap"));
					if (!MapProperty) { Test->AddError(TEXT("Lobby GameMode has no GameplayMap property.")); return true; }
					const FSoftObjectPtr PreviousMap = MapProperty->GetPropertyValue_InContainer(Lobby);
					Test->TestFalse(TEXT("Production Blueprint configures a gameplay destination"), PreviousMap.IsNull());
					Test->AddInfo(FString::Printf(TEXT("Production lobby Blueprint GameplayMap=%s"), *PreviousMap.ToSoftObjectPath().ToString()));
					MapProperty->SetPropertyValue_InContainer(Lobby, FSoftObjectPtr(FSoftObjectPath(Destination)));
					StartButton->OnClicked.Broadcast();
					MapProperty->SetPropertyValue_InContainer(Lobby, PreviousMap);
					Test->TestTrue(TEXT("Production start click records the host's start confirmation"), HostState->IsStartConfirmed());
					Test->TestFalse(TEXT("Accepted start immediately disables the button"), StartButton->GetIsEnabled());
				}
                else if (!Lobby->RequestMatchTravel(TSoftObjectPtr<UWorld>(FSoftObjectPath(Destination))))
                {
                    Test->AddError(Sessions->GetServerTravelError());
                    return true;
                }
                Test->TestTrue(TEXT("Travel closes admission immediately"), Sessions->IsMatchAdmissionClosed());
                Next(EPhase::Travel);
                return false;
            }

            if (Sessions->GetServerTravelState() == EHBServerTravelState::Failed
                || Sessions->GetServerTravelState() == EHBServerTravelState::RestoringLobby)
            {
                Test->AddError(Sessions->GetServerTravelError());
                return true;
            }
            if (Phase == EPhase::Travel)
            {
                if (Sessions->GetServerTravelState() != EHBServerTravelState::Complete)
                    return Wait(TEXT("Steam lobby-to-dungeon seamless travel did not complete."));
                Test->TestEqual(TEXT("Gameplay retains the same SteamSockets driver"), World->GetNetDriver(), Fixture->LobbyDriver.Get());
                Test->TestEqual(TEXT("Gameplay is still a listen server"), World->GetNetMode(), NM_ListenServer);
				if (Fixture->bLobbyUI)
					Test->TestTrue(TEXT("Lobby departure removes the old HUD"), !Fixture->LobbyHUD.IsValid() || !Fixture->LobbyHUD->IsInViewport());
                FNamedOnlineSession* Session = Online->GetNamedSession(SteamTestSession);
                if (!Mode || !Session)
                { Test->AddError(TEXT("Travel lost the gameplay GameMode or original Steam session.")); return true; }
                Test->TestEqual(TEXT("Travel retains the original Steam lobby ID"), Session->GetSessionIdStr(), Fixture->SessionId);
                FString AdvertisedMap;
                Session->SessionSettings.Get(FName(TEXT("HB_MAP")), AdvertisedMap);
                Test->TestEqual(TEXT("Steam metadata identifies the destination map"), AdvertisedMap,
                    FSoftObjectPath(Fixture->bSessionOnly ? TEXT("/Engine/Maps/Entry.Entry") : DungeonMap).GetLongPackageName());
                Test->TestTrue(TEXT("Match Steam lobby disallows joining and invites"),
                    Sessions->IsMatchAdmissionClosed() && !Session->SessionSettings.bAllowJoinInProgress
                    && !Session->SessionSettings.bAllowJoinViaPresence && !Session->SessionSettings.bAllowInvites);
                if (Fixture->bAutomaticHub)
                    Test->TestTrue(TEXT("Match retains the friends-only Steam lobby type"),
                        Session->SessionSettings.bShouldAdvertise && Session->SessionSettings.bAllowJoinViaPresenceFriendsOnly);
                if (Fixture->bSessionOnly)
                {
                    auto* Player = World->GetFirstPlayerController();
                    auto* PlayerState = Player ? Player->GetPlayerState<AHB_PlayerState>() : nullptr;
                    Test->TestTrue(TEXT("Session-only seamless travel retains the lobby loadout"),
                        PlayerState && PlayerState->GetStartingLoadout() == Fixture->LobbyLoadout);
                    Fixture->EndingActors = {Mode};
                    if (Fixture->EndReason == EHBPostMatchReason::MatchEnded)
                        FindFProperty<FFloatProperty>(Mode->GetClass(), TEXT("MatchDurationSeconds"))->SetPropertyValue_InContainer(Mode, 2.f);
                    Mode->StartMatch();
                    Next(EPhase::Match);
                    return false;
                }
                Next(EPhase::Prepare);
            }

            AHBDungeonRuntime* Runtime = Mode ? Mode->GetDungeonGenerator() : nullptr;
            if (!Fixture->bSessionOnly)
            {
                if (!Runtime) return Wait(TEXT("Steam gameplay world has no dungeon runtime."));
                if (Runtime->State == EHBDungeonRuntimeState::Failed)
                { Test->AddError(Runtime->Status); return true; }
                if (!Runtime->IsReady()) return Wait(FString::Printf(TEXT("Steam host dungeon preparation timed out: %s"), *Runtime->Status));
            }

            if (Phase == EPhase::Prepare)
            {
                auto* State = World->GetGameState<AHeavenBreachGameState>();
                auto* Player = World->GetFirstPlayerController();
                auto* PlayerState = Player ? Player->GetPlayerState<AHB_PlayerState>() : nullptr;
                if (!State || !PlayerState)
                { Test->AddError(TEXT("Steam gameplay host lost GameState or PlayerState.")); return true; }
                const FHBDungeonSnapshot& Snapshot = State->GetDungeonSnapshot();
                Test->TestTrue(TEXT("Steam host publishes an active dungeon snapshot"),
                    Snapshot.State == EHBDungeonSnapshotState::Active && Snapshot.GenerationId == Runtime->GetGenerationId()
                    && Snapshot.GenerationId > 0 && Snapshot.Rooms.Num() == Runtime->Rooms.Num() && !Snapshot.Rooms.IsEmpty());
                Test->TestTrue(TEXT("Seamless travel retains the lobby loadout"), PlayerState->GetStartingLoadout() == Fixture->LobbyLoadout);
                Test->TestTrue(TEXT("Local Steam host passes the dungeon loading gate"), Mode->AreDungeonPlayersLoaded());
                Test->TestFalse(TEXT("AI waits for match start on the Steam host"), Runtime->AreEncountersActive());
                auto* Encounters = World->GetSubsystem<UHBEncounterSubsystem>();
                auto* Chests = World->GetSubsystem<UHBChestSpawnSubsystem>();
                int32 EnemyCount = 0, ChestCount = 0;
                for (const FHBPrototypeRoom& Room : Runtime->Rooms)
                {
                    ULevel* Level = Runtime->GetLoadedRoomLevel(Room.Id);
                    Test->TestNotNull(TEXT("Every Steam host room instance is loaded"), Level);
                    if (Encounters && Encounters->HasEncounter(Level))
                    {
                        Test->TestTrue(TEXT("Steam room encounter prepared"), Encounters->IsPreparedFor(Snapshot.GenerationId, Room.Id, Level));
                        EnemyCount += Encounters->GetAliveCount(Snapshot.GenerationId, Room.Id, Level);
                    }
                    if (Chests && Chests->HasPlacement(Level))
                    {
                        Test->TestTrue(TEXT("Steam room chest placement prepared"), Chests->IsPreparedFor(Snapshot.GenerationId, Room.Id, Level));
                        ChestCount += Chests->GetSpawnedCount(Snapshot.GenerationId, Room.Id, Level);
                    }
                }
                Test->TestTrue(TEXT("Steam dungeon contains actual spawned enemies"), EnemyCount > 0);
                Test->TestTrue(TEXT("Steam dungeon contains actual spawned chests"), ChestCount > 0);
                Fixture->EndingActors = {Runtime, Mode};
                for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It) Fixture->EndingActors.Add(*It);
                for (TActorIterator<ABoxActor> It(World); It; ++It) Fixture->EndingActors.Add(*It);
                Test->AddInfo(FString::Printf(TEXT("Steam dungeon ready: %d rooms, %d enemies, %d chests."), Runtime->Rooms.Num(), EnemyCount, ChestCount));
                if (Fixture->EndReason == EHBPostMatchReason::MatchEnded)
                    FindFProperty<FFloatProperty>(Mode->GetClass(), TEXT("MatchDurationSeconds"))->SetPropertyValue_InContainer(Mode, 2.f);
                Mode->StartMatch();
                Next(EPhase::Match);
                return false;
            }

            auto* Player = World->GetFirstPlayerController();
            if (!Mode || !Mode->IsMatchInProgress() ||
                (!Fixture->bSessionOnly && !Runtime->AreEncountersActive()) || !Player || !Player->GetPawn())
                return Wait(TEXT("Steam host did not start gameplay after dungeon preparation."));
            if (Fixture->EndReason != EHBPostMatchReason::None)
            {
                FString LoginError;
                Mode->PreLogin(TEXT(""), TEXT("test-remote-player"), FUniqueNetIdRepl(OSS->GetIdentityInterface()->GetUniquePlayerId(0)), LoginError);
                Test->TestEqual(TEXT("Active network match still rejects new admissions"), LoginError, FString(TEXT("Match admission is closed")));
                if (Fixture->EndReason == EHBPostMatchReason::Died)
                    Mode->NotifyPlayerDied(Player->PlayerState);
                else if (Fixture->EndReason == EHBPostMatchReason::Extracted)
                    Mode->NotifyPlayerExtracted(Player->PlayerState);
                // MatchEnded uses the real two-second match timer, not EndMatch.
                Next(EPhase::Result);
                return false;
            }
            Test->AddInfo(TEXT("Steam host match started with active encounters and a possessed player pawn. Remote Steam joining/replication remains unverified."));
            if (Fixture->bAutomaticHub)
            {
                if (!Sessions->RequestReturnToOwnHub())
                { Test->AddError(TEXT("Gameplay could not request personal hub return.")); return true; }
                Next(EPhase::ReturnHub);
                return false;
            }
            return true;
        }
    };

    // Always queued after the verification command, including on failure.
    // Leave the real Steam lobby before unloading the generated dungeon world.
    class FHBSteamHostCleanup : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FHBSteamHostFixture> Fixture;
        double Started = 0.;
        bool bLeaveRequested = false;
        bool bMapRequested = false;
    public:
        FHBSteamHostCleanup(FAutomationTestBase* InTest, TSharedRef<FHBSteamHostFixture> InFixture)
            : Test(InTest), Fixture(InFixture) {}
        virtual bool Update() override
        {
            // The verification command may itself wait several minutes.
            // Cleanup gets its own deadline when it actually starts running.
            if (Started == 0.)
            {
                Started = FPlatformTime::Seconds();
                FCoreUObjectDelegates::PreLoadMapWithContext.Remove(Fixture->ResultDepartureHandle);
                Fixture->ResultDepartureHandle.Reset();
                FWorldDelegates::OnWorldInitializedActors.Remove(Fixture->GameplayInitializationHandle);
                Fixture->GameplayInitializationHandle.Reset();
            }
            if (FPlatformTime::Seconds() - Started > 180.)
            { Test->AddError(TEXT("Steam test cleanup timed out; inspect lobby/travel cleanup errors.")); return true; }
            auto* Instance = Fixture->Instance.Get();
            if (!Instance) return true;
            auto* Sessions = Instance->GetSubsystem<UHBOnlineSessionSubsystem>();
            IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
            const IOnlineSessionPtr Online = OSS ? OSS->GetSessionInterface() : nullptr;
            if (!Sessions || !Online.IsValid())
            { Test->AddError(TEXT("Steam test cleanup lost its session interface.")); return true; }
            if (Sessions->IsLobbyOperationInProgress()) return false;
            if (Online->GetNamedSession(SteamTestSession))
            {
                if (!bLeaveRequested) { bLeaveRequested = true; Sessions->LeaveLobby(); }
                return false;
            }
            if (!bMapRequested)
            {
                bMapRequested = true;
                UGameplayStatics::OpenLevel(Instance, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("game=/Script/Engine.GameModeBase"));
                return false;
            }
            UWorld* World = Instance->GetWorld();
            if (!World || !World->HasBegunPlay() || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return false;
            Test->TestEqual(TEXT("Cleanup resets the Steam lobby operation"), Sessions->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
            Test->TestEqual(TEXT("Cleanup resets the Steam travel operation"), Sessions->GetServerTravelState(), EHBServerTravelState::Idle);
            Test->TestFalse(TEXT("Cleanup releases the admission gate"), Sessions->IsMatchAdmissionClosed());
            Test->TestEqual(TEXT("Cleanup releases the listen connection"), World->GetNetMode(), NM_Standalone);
            Test->TestTrue(TEXT("Cleanup destroys the previous Steam listen driver"), !Fixture->LobbyDriver.IsValid());
            for (const auto& Actor : Fixture->EndingActors)
                Test->TestTrue(TEXT("Leaving Steam gameplay invokes EndPlay for generated actors"), !Actor.IsValid() || !Actor->HasActorBegunPlay());
            Test->AddInfo(TEXT("Real Steam test lobby removed and dungeon world unloaded."));
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonSteamHostTravelTest, "HeavenBreach.PCGModule.Steam.ListenHostTravel",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHBDungeonSteamHostTravelTest::RunTest(const FString&)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("HBSteamListenTest")))
    { AddError(TEXT("Real Steam test requires explicit -HBSteamListenTest opt-in and a logged-in Steam client.")); return false; }
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || OSS->GetAppId() != TEXT("480"))
    { AddError(TEXT("This development smoke test requires initialized Steam OSS with AppId 480.")); return false; }
    UGameInstance* Instance = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game && Context.World()) Instance = Context.World()->GetGameInstance();
    auto* LocalPlayer = Instance ? Instance->GetFirstGamePlayer() : nullptr;
    auto* Sessions = Instance ? Instance->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
    const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    const IOnlineSessionPtr Online = OSS->GetSessionInterface();
    if (!LocalPlayer || !Sessions || !Identity.IsValid() || !Online.IsValid()
        || Identity->GetLoginStatus(LocalPlayer->GetControllerId()) != ELoginStatus::LoggedIn
        || Instance->GetWorld()->GetNetMode() != NM_Standalone
        || Instance->GetWorld()->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")
        || Online->GetNamedSession(SteamTestSession) || Online->GetNamedSession(NAME_GameSession))
    { AddError(TEXT("Start -game on /Engine/Maps/Entry with a logged-in Steam local player and no existing session.")); return false; }

    AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    const auto Fixture = MakeShared<FHBSteamHostFixture>();
    Fixture->bAutomaticHub = FParse::Param(FCommandLine::Get(), TEXT("HBTestAutoHub"));
    Fixture->bSessionOnly = FParse::Param(FCommandLine::Get(), TEXT("HBTestSessionOnly"));
	Fixture->bLobbyUI = FParse::Param(FCommandLine::Get(), TEXT("HBTestLobbyUI"));
	if (Fixture->bLobbyUI && !Fixture->bAutomaticHub)
	{ AddError(TEXT("HBTestLobbyUI requires HBTestAutoHub.")); return false; }
    if (!Fixture->bSessionOnly)
    {
        AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
        AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
    }
    Fixture->Instance = Instance;
    FString EndCase;
    if (FParse::Value(FCommandLine::Get(), TEXT("HBTestPostMatch="), EndCase))
    {
        if (EndCase == TEXT("Died")) Fixture->EndReason = EHBPostMatchReason::Died;
        else if (EndCase == TEXT("Extracted")) Fixture->EndReason = EHBPostMatchReason::Extracted;
        else if (EndCase == TEXT("TimeExpired")) Fixture->EndReason = EHBPostMatchReason::MatchEnded;
        else { AddError(TEXT("HBTestPostMatch must be Died, Extracted or TimeExpired.")); return false; }
        // Existing result-map player initialization loses its job on absolute
        // travel and falls back to the default loadout. Report outside scope.
        AddExpectedErrorPlain(TEXT("[SkillLoadout] 전달된 시작 구성을 쓸 수 없다: 시작 구성의 직업이 적용 직업과 다릅니다."), EAutomationExpectedErrorFlags::Exact, 1);
        AddWarning(TEXT("Existing ExtractionResult player loadout mismatch is outside session scope; player/UI assets are unchanged."));
        Fixture->ResultDepartureHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddLambda(
            [Fixture](const FWorldContext& Context, const FString& MapName)
            {
                if (Context.OwningGameInstance == Fixture->Instance.Get() && MapName == TEXT("/Game/Levels/ExtractionResult"))
                    if (const auto* Result = Fixture->Instance->GetSubsystem<UHBMatchResultSubsystem>())
                        Fixture->ObservedResultReason = Result->GetPostMatchReason();
            });
    }
    if (Fixture->bSessionOnly)
    {
        Fixture->GameplayInitializationHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda(
            [Fixture](const FActorsInitializedParams& Params)
            {
                UWorld* World = Params.World;
                if (!World || World->GetGameInstance() != Fixture->Instance.Get() ||
                    UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TEXT("/Engine/Maps/Entry")) return;
                if (auto* Mode = World->GetAuthGameMode<AHeavenBreachGameMode>())
                {
                    // Empty maps can start in PostLogin, before the latent command
                    // observes travel completion. Configure this fixture beforehand.
                    FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, false);
                    if (Fixture->EndReason == EHBPostMatchReason::MatchEnded)
                        FindFProperty<FFloatProperty>(Mode->GetClass(), TEXT("MatchDurationSeconds"))->SetPropertyValue_InContainer(Mode, 2.f);
                }
            });
    }
    bool bAccepted = true;
    if (Fixture->bAutomaticHub)
        UGameplayStatics::OpenLevel(Instance, FName(*FSoftObjectPath(LobbyMap).GetLongPackageName()), true);
    else
        bAccepted = Sessions->RequestHostLobby(2, TSoftObjectPtr<UWorld>(FSoftObjectPath(LobbyMap)));
    if (bAccepted)
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBSteamHostTravel>(this, Fixture));
    else AddError(Sessions->GetLastLobbyOperationError());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBSteamHostCleanup>(this, Fixture));
    return bAccepted;
}
#endif
