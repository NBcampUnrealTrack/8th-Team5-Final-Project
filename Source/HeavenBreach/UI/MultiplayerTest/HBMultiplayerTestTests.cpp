#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/MultiplayerTest/HBMultiplayerTestWidget.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Core/Online/Session/HBLobbyProtocol.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRoomProtocolTest, "HeavenBreach.Multiplayer.RoomProtocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBRoomProtocolTest::RunTest(const FString&)
{
    TestTrue(TEXT("Korean room names accepted"), HBLobbyProtocol::IsValidRoomName(TEXT("친구들과 플레이")));
    TestFalse(TEXT("Whitespace-only room name rejected"), HBLobbyProtocol::IsValidRoomName(TEXT("  ")));
    TestFalse(TEXT("Multiline room name rejected"), HBLobbyProtocol::IsValidRoomName(TEXT("a\nb")));
    TestFalse(TEXT("Oversized room name rejected"), HBLobbyProtocol::IsValidRoomName(FString::ChrN(49, TEXT('x'))));
    const FString Proof = HBLobbyProtocol::PasswordProof(TEXT("비밀?&="), TEXT("room-a"));
    TestFalse(TEXT("Wrong password cannot authenticate"), Proof == HBLobbyProtocol::PasswordProof(TEXT("wrong"), TEXT("room-a")));
    TestFalse(TEXT("Proof from another room cannot authenticate"), Proof == HBLobbyProtocol::PasswordProof(TEXT("비밀?&="), TEXT("room-b")));
    TestEqual(TEXT("Proof is URL-safe hex"), Proof.Len(), 40);
    TestTrue(TEXT("Open room has no password credential"), HBLobbyProtocol::PasswordProof(FString(), TEXT("room-a")).IsEmpty());
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBHostStartPolicyTest, "HeavenBreach.Multiplayer.HostStartPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBHostStartPolicyTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* State = World->SpawnActor<AHBLobbyGameState>();
    auto* Host = World->SpawnActor<AHBLobbyPlayerState>();
    auto* Guest = World->SpawnActor<AHBLobbyPlayerState>();
    State->HostPlayerState = Host;
    State->PlayerArray = {Host};
    TestTrue(TEXT("A lone host can start without Ready"), State->HasValidStartRoster());
    State->PlayerArray.Add(Guest);
    TestTrue(TEXT("An unready guest never blocks host start"), State->HasValidStartRoster());
    Guest->SetReady(true);
    TestFalse(TEXT("Host never needs to toggle Ready"), Host->IsReady());
    TestTrue(TEXT("Legacy guest Ready does not change start eligibility"), State->HasValidStartRoster());
    Guest->SetReady(false);
    TestTrue(TEXT("Legacy Ready cancellation never blocks host start"), State->HasValidStartRoster());
    for (int32 Index = 0; Index < 2; ++Index)
    {
        State->PlayerArray.Add(World->SpawnActor<AHBLobbyPlayerState>());
    }
    TestTrue(TEXT("A full four-player roster can start without anyone Ready"), State->HasValidStartRoster());
    State->PlayerArray.Add(World->SpawnActor<AHBLobbyPlayerState>());
    TestFalse(TEXT("More than four players cannot start"), State->HasValidStartRoster());
    State->PlayerArray = {Host};
    TestTrue(TEXT("Host can start alone after guests leave"), State->HasValidStartRoster());
    State->PlayerArray = {Guest};
    TestFalse(TEXT("Host must remain in the roster"), State->HasValidStartRoster());
    State->HostPlayerState = nullptr;
    TestFalse(TEXT("An absent host cannot start"), State->HasValidStartRoster());
    World->DestroyWorld(false);
    return true;
}
#endif

namespace
{
    UHBMultiplayerTestWidget* FindScreen(UWorld* World)
    {
        for (TObjectIterator<UHBMultiplayerTestWidget> It; It; ++It)
            if (It->GetWorld() == World && It->IsInViewport()) return *It;
        return nullptr;
    }
    bool Click(FAutomationTestBase* Test, UHBMultiplayerTestWidget* Screen, const TCHAR* Name)
    {
        auto* Button = Screen ? Cast<UButton>(Screen->GetWidgetFromName(FName(Name))) : nullptr;
        if (!Button || !Button->GetIsEnabled()) { Test->AddError(FString::Printf(TEXT("UI button missing/disabled: %s"), Name)); return false; }
        Button->OnClicked.Broadcast();
        return true;
    }
    class FHBSteamUIExercise : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TWeakObjectPtr<UGameInstance> Instance;
        enum class EPhase { Host, Friends, SoloMatch, Menu, Search } Phase = EPhase::Host;
        double Started = FPlatformTime::Seconds();
        bool bFailed = false;
        bool bCapturedLobby = false;
    public:
        FHBSteamUIExercise(FAutomationTestBase* InTest, UGameInstance* InInstance) : Test(InTest), Instance(InInstance) {}
        virtual bool Update() override
        {
            auto* GI = Instance.Get();
            auto* World = GI ? GI->GetWorld() : nullptr;
            auto* Service = GI ? GI->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
            auto* Screen = World ? FindScreen(World) : nullptr;
            if (FPlatformTime::Seconds() - Started > 90.)
            {
                Test->AddError(TEXT("Steam UI host/leave exercise timed out."));
                if (Service) Service->RequestLeaveToMenu();
                return true;
            }
            if (!World || !Service || (!Screen && Phase != EPhase::SoloMatch)) return false;
            if (Service->GetLobbyOperationState() == EHBLobbyOperationState::Failed && !bFailed)
            {
                bFailed = true; Test->AddError(Service->GetLastLobbyOperationError());
                Service->RequestLeaveToMenu(); Phase = EPhase::Menu;
            }
            if (Phase == EPhase::Host)
            {
                if (Service->GetLobbyOperationState() != EHBLobbyOperationState::InLobby) return false;
                if (FParse::Param(FCommandLine::Get(), TEXT("HBTestScreenshots")))
                {
                    if (!bCapturedLobby)
                    {
                        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MultiplayerTestLobby.png"), true, false);
                        bCapturedLobby = true;
                        return false;
                    }
                    if (FScreenshotRequest::IsScreenshotRequested()) return false;
                }
                const auto* State = World->GetGameState<AHBLobbyGameState>();
                auto* Mode = World->GetAuthGameMode<AHBLobbyGameMode>();
                auto* OSS = IOnlineSubsystem::Get();
                const auto Online = OSS->GetSessionInterface();
                const auto* Session = Online->GetNamedSession(FName(TEXT("HBPartyLobbySession")));
                if (!State || !Mode || !Session) { Test->AddError(TEXT("UI host missing its live room state.")); return true; }
                Test->TestEqual(TEXT("Entered room name replicates through GameState"), State->RoomName, FString(TEXT("UI 기능 테스트")));
                Test->TestEqual(TEXT("UI creates exactly four slots"), Session->SessionSettings.NumPublicConnections, 4);
                auto* Start = Cast<UButton>(Screen->GetWidgetFromName(TEXT("StartMatchButton")));
                // Native UI refreshes periodically after host creation completes.
                if (Start && !Start->GetIsEnabled()) return false;
                Test->TestTrue(TEXT("A single host can start without any guests"), Start && Start->GetIsEnabled());
                FString Salt; Session->SessionSettings.Get(FName(TEXT("HB_SALT")), Salt);
                FString Error;
                const auto UserId = OSS->GetIdentityInterface()->GetUniquePlayerId(0);
                Mode->PreLogin(TEXT("?HBProof=wrong"), TEXT("fixture"), FUniqueNetIdRepl(UserId), Error);
                Test->TestEqual(TEXT("Real lobby PreLogin rejects wrong passwords"), Error, FString(TEXT("HB_PASSWORD_INVALID")));
                Mode->PreLogin(TEXT("?HBProof=") + HBLobbyProtocol::PasswordProof(TEXT("test?한글=123"), Salt), TEXT("fixture"), FUniqueNetIdRepl(UserId), Error);
                Test->TestTrue(TEXT("Correct password passes actual server PreLogin"), Error.IsEmpty());
                const int32 OriginalPlayerCount = Mode->NumPlayers;
                Mode->NumPlayers = 4;
                Mode->PreLogin(TEXT("?HBProof=") + HBLobbyProtocol::PasswordProof(TEXT("test?한글=123"), Salt), TEXT("fixture"), FUniqueNetIdRepl(UserId), Error);
                Mode->NumPlayers = OriginalPlayerCount;
                Test->TestEqual(TEXT("Server PreLogin rejects a fifth player even with the correct password"), Error, FString(TEXT("HB_ROOM_FULL")));
                Test->TestFalse(TEXT("Password proof is never published as Steam metadata"), Session->SessionSettings.Settings.Contains(FName(TEXT("HBProof"))));
                if (!Click(Test, Screen, TEXT("OpenFriendsButton"))) return true;
                Phase = EPhase::Friends; Started = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == EPhase::Friends)
            {
                if (Service->IsReadingFriends()) return false;
                if (!Service->GetLastLobbyOperationError().IsEmpty())
                {
                    Test->AddError(Service->GetLastLobbyOperationError());
                    Service->RequestLeaveToMenu(); Phase = EPhase::Menu; bFailed = true;
                    return false;
                }
                if (!Click(Test, Screen, TEXT("FriendsBackButton"))) return true;
                if (FParse::Param(FCommandLine::Get(), TEXT("HBTestSoloStart")))
                {
                    if (!Click(Test, Screen, TEXT("StartMatchButton"))) return true;
                    Phase = EPhase::SoloMatch; Started = FPlatformTime::Seconds();
                    return false;
                }
                if (!Click(Test, Screen, TEXT("LeaveRoomButton"))) return true;
                Phase = EPhase::Menu; Started = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == EPhase::SoloMatch)
            {
                if (Service->GetServerTravelState() == EHBServerTravelState::Failed)
                {
                    Test->AddError(Service->GetServerTravelError());
                    Service->RequestLeaveToMenu(); Phase = EPhase::Menu; bFailed = true;
                    return false;
                }
                if (Service->GetServerTravelState() != EHBServerTravelState::Complete) return false;
                Test->TestEqual(TEXT("Solo Start button travels to the gameplay map"), World->GetOutermost()->GetName(), FString(TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay")));
                Test->TestEqual(TEXT("Solo gameplay retains its listen server"), World->GetNetMode(), NM_ListenServer);
                Test->TestTrue(TEXT("Starting solo closes admission"), Service->IsMatchAdmissionClosed());
                Test->TestNull(TEXT("Solo gameplay removes the lobby screen"), FindScreen(World));
                Test->TestTrue(TEXT("Solo gameplay releases the UI input block"), World->GetGameViewport() && !World->GetGameViewport()->IgnoreInput());
                Test->AddInfo(TEXT("Actual Steam UI Start button verified solo host travel to gameplay."));
                Service->RequestLeaveToMenu(); Phase = EPhase::Menu; Started = FPlatformTime::Seconds();
                return false;
            }
            if (World->GetNetMode() != NM_Standalone || World->GetOutermost()->GetName() != TEXT("/Game/Levels/Test_CW/Multiplayer/L_MultiplayerTestMenu")) return false;
            if (Phase == EPhase::Search && Service->IsLobbyOperationInProgress()) return false;
            if (Phase == EPhase::Menu)
                Test->TestNotNull(TEXT("Leaving restores the functional main menu"), Screen->GetWidgetFromName(TEXT("OpenCreateButton")));
            Test->TestEqual(TEXT("Leaving resets the operation"), Service->GetLobbyOperationState(), EHBLobbyOperationState::Idle);
            Test->TestNull(TEXT("Leaving removes the real Steam lobby"), IOnlineSubsystem::Get()->GetSessionInterface()->GetNamedSession(FName(TEXT("HBPartyLobbySession"))));
            if (!bFailed && Phase == EPhase::Menu)
            {
                if (!Click(Test, Screen, TEXT("OpenFindButton"))) return true;
                Phase = EPhase::Search; Started = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == EPhase::Search)
            {
                Test->TestNotNull(TEXT("Search screen has a refresh action"), Screen->GetWidgetFromName(TEXT("RefreshRoomsButton")));
                if (!Click(Test, Screen, TEXT("FindBackButton"))) return true;
            }
            Test->AddInfo(TEXT("Actual UMG buttons verified protected Steam room creation, server password admission, four slots, friends screen and return to menu."));
            return true;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSteamUITest, "HeavenBreach.Multiplayer.SteamUIHost",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBSteamUITest::RunTest(const FString&)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    { AddError(TEXT("UMG UI tests require rendering for widget ticks; use -RenderOffscreen instead of -nullrhi.")); return false; }
    auto* OSS = IOnlineSubsystem::Get();
    if (!FParse::Param(FCommandLine::Get(), TEXT("HBSteamListenTest")) || !OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")) || OSS->GetAppId() != TEXT("480"))
    { AddError(TEXT("Steam UI test requires -game -HBSteamListenTest, logged-in Steam and AppId 480.")); return false; }
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
    auto* Screen = World ? FindScreen(World) : nullptr;
    if (!Screen || World->GetNetMode() != NM_Standalone || World->GetOutermost()->GetName() != TEXT("/Game/Levels/Test_CW/Multiplayer/L_MultiplayerTestMenu"))
    { AddError(TEXT("Start Steam UI test on L_MultiplayerTestMenu.")); return false; }
    if (FParse::Param(FCommandLine::Get(), TEXT("HBTestSoloStart")))
    {
        // Existing gameplay asset errors are reported separately from lobby logic.
        AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    }
    // Revisit the panel to verify that native widget reconstruction keeps its actions bound.
    if (!Click(this, Screen, TEXT("OpenCreateButton")) || !Click(this, Screen, TEXT("CancelCreateButton"))) return false;
    if (!Click(this, Screen, TEXT("OpenCreateButton"))) return false;
    CastChecked<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("RoomNameInput")))->SetText(FText::FromString(TEXT("UI 기능 테스트")));
    CastChecked<UCheckBox>(Screen->GetWidgetFromName(TEXT("UsePasswordCheck")))->SetIsChecked(true);
    CastChecked<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("RoomPasswordInput")))->SetText(FText::FromString(TEXT("test?한글=123")));
    if (FParse::Param(FCommandLine::Get(), TEXT("HBTestScreenshots")))
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MultiplayerTestCreate.png"), true, false);
    if (!Click(this, Screen, TEXT("CreateRoomButton"))) return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBSteamUIExercise>(this, World->GetGameInstance()));
    return true;
}
#endif
