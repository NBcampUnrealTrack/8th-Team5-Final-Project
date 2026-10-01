#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
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

namespace
{
    const FName SteamTestSession(TEXT("HBPartyLobbySession"));
    const TCHAR* LobbyMap = TEXT("/Game/Levels/Lobby/L_Lobby.L_Lobby");
    const TCHAR* DungeonMap = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay.Test_MGGamePlay");
    const TCHAR* SteamDriverClass = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");

    struct FHBSteamHostFixture
    {
        TWeakObjectPtr<UGameInstance> Instance;
        TWeakObjectPtr<UNetDriver> LobbyDriver;
        TArray<TWeakObjectPtr<AActor>> EndingActors;
        FHBStartingLoadout LobbyLoadout;
        FString SessionId;
    };

    // Explicitly opted-in, standalone -game test. It creates one real Steam
    // lobby, not a mock/IP session. A second Steam identity is still needed to
    // verify search, joining and replication across a real client connection.
    class FHBSteamHostTravel : public IAutomationLatentCommand
    {
        enum class EPhase { Host, Travel, Prepare, Match };
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
                Test->TestTrue(TEXT("Steam session belongs to the logged-in host"),
                    LocalId.IsValid() && Session->OwningUserId.IsValid() && *LocalId == *Session->OwningUserId);
                Test->TestTrue(TEXT("Host uses a non-LAN, non-dedicated presence lobby"),
                    !Session->SessionSettings.bIsLANMatch && !Session->SessionSettings.bIsDedicated
                    && Session->SessionSettings.bUsesPresence && Session->SessionSettings.bUseLobbiesIfAvailable);
                Fixture->LobbyDriver = Driver;
                Fixture->LobbyLoadout = State->GetStartingLoadout();
                Fixture->SessionId = Session->GetSessionIdStr();
                Test->TestFalse(TEXT("Steam assigned a real lobby ID"), Fixture->SessionId.IsEmpty());
                // UE 5.7 Steam OSS populates OwningUserId/HostingPlayerNum but
                // leaves bHosting false. Do not manufacture that flag here:
                // the public travel API must accept an actual Steam host.
                Test->AddInfo(FString::Printf(TEXT("Real Steam lobby and SteamSockets listen driver verified; bHosting=%d; requesting internal match travel."), Session->bHosting));
                if (!Lobby->RequestMatchTravel(TSoftObjectPtr<UWorld>(FSoftObjectPath(DungeonMap))))
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
                FNamedOnlineSession* Session = Online->GetNamedSession(SteamTestSession);
                if (!Mode || !Session)
                { Test->AddError(TEXT("Travel lost the gameplay GameMode or original Steam session.")); return true; }
                Test->TestEqual(TEXT("Travel retains the original Steam lobby ID"), Session->GetSessionIdStr(), Fixture->SessionId);
                FString AdvertisedMap;
                Session->SessionSettings.Get(FName(TEXT("HB_MAP")), AdvertisedMap);
                Test->TestEqual(TEXT("Steam metadata identifies the destination map"), AdvertisedMap, FSoftObjectPath(DungeonMap).GetLongPackageName());
                Test->TestTrue(TEXT("Match Steam lobby disallows joining and invites"),
                    Sessions->IsMatchAdmissionClosed() && !Session->SessionSettings.bAllowJoinInProgress
                    && !Session->SessionSettings.bAllowJoinViaPresence && !Session->SessionSettings.bAllowInvites);
                Next(EPhase::Prepare);
            }

            AHBDungeonRuntime* Runtime = Mode ? Mode->GetDungeonGenerator() : nullptr;
            if (!Runtime) return Wait(TEXT("Steam gameplay world has no dungeon runtime."));
            if (Runtime->State == EHBDungeonRuntimeState::Failed)
            { Test->AddError(Runtime->Status); return true; }
            if (!Runtime->IsReady()) return Wait(FString::Printf(TEXT("Steam host dungeon preparation timed out: %s"), *Runtime->Status));

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
                Mode->StartMatch();
                Next(EPhase::Match);
                return false;
            }

            auto* Player = World->GetFirstPlayerController();
            if (!Mode->IsMatchInProgress() || !Runtime->AreEncountersActive() || !Player || !Player->GetPawn())
                return Wait(TEXT("Steam host did not start gameplay after dungeon preparation."));
            Test->AddInfo(TEXT("Steam host match started with active encounters and a possessed player pawn. Remote Steam joining/replication remains unverified."));
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
            if (Started == 0.) Started = FPlatformTime::Seconds();
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
    AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
    const auto Fixture = MakeShared<FHBSteamHostFixture>();
    Fixture->Instance = Instance;
    const bool bAccepted = Sessions->RequestHostLobby(2, TSoftObjectPtr<UWorld>(FSoftObjectPath(LobbyMap)));
    if (bAccepted)
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBSteamHostTravel>(this, Fixture));
    else AddError(Sessions->GetLastLobbyOperationError());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBSteamHostCleanup>(this, Fixture));
    return bAccepted;
}
#endif
