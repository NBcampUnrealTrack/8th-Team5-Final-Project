#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "AI/EnemyCharacterBase.h"
#include "Loot/BoxActor.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"

namespace
{
    AHBDungeonRuntime* FindDungeon(UWorld* World)
    {
        if (World)
            for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) return *It;
        return nullptr;
    }

    struct FHBReplicationFixture
    {
        EPlayNetMode Mode;
        int32 Clients = 1;
        bool bOneProcess = true;
        bool bSeparateServer = false;
        TArray<FNetDriverDefinition> Drivers;
        bool bConfigured = false;

        void Restore() const
        {
            GEngine->NetDriverDefinitions = Drivers;
            auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
            Settings->SetPlayNetMode(Mode);
            Settings->SetPlayNumberOfClients(Clients);
            Settings->SetRunUnderOneProcess(bOneProcess);
            Settings->bLaunchSeparateServer = bSeparateServer;
            // Ending PIE saves these preferences; persist the restored values too.
            Settings->SaveConfig();
        }
    };

    // Real sockets and two PIE worlds exercise actor channels and dynamic room
    // package remapping. Steam lobby/transport verification is a separate test.
    class FHBWaitForDungeonReplication : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FHBReplicationFixture> Fixture;
        double Started = FPlatformTime::Seconds();
        double PhaseStarted = 0;
        int32 RoomIndex = 0;
        int32 VerifiedEnemies = 0;
        int32 VerifiedChests = 0;
        bool bFocused = false;
        bool bExercised = false;
        bool bClearing = false;
        TArray<TWeakObjectPtr<AActor>> ServerActors;
        TArray<TWeakObjectPtr<AActor>> ClientActors;

        bool WaitOrFail(const FString& Reason)
        {
            if (FPlatformTime::Seconds() - (PhaseStarted > 0 ? PhaseStarted : Started) < 90.) return false;
            Test->AddError(Reason);
            return true;
        }

    public:
        FHBWaitForDungeonReplication(FAutomationTestBase* InTest, TSharedRef<FHBReplicationFixture> InFixture)
            : Test(InTest), Fixture(InFixture) {}

        virtual bool Update() override
        {
            if (!Fixture->bConfigured) return true;
            UWorld* Server = nullptr;
            UWorld* Client = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (Context.WorldType != EWorldType::PIE || !World) continue;
                if (World->GetNetMode() == NM_ListenServer) Server = World;
                if (World->GetNetMode() == NM_Client) Client = World;
            }
            // PIE can recompile the map's GameMode Blueprint, replacing its CDO.
            // Pause the actual server instance before client login/room readiness.
            if (Server)
                if (auto* ServerMode = Server->GetAuthGameMode<AHeavenBreachGameMode>())
                    FindFProperty<FBoolProperty>(ServerMode->GetClass(), TEXT("bAutoStartMatch"))
                        ->SetPropertyValue_InContainer(ServerMode, false);
            if (!Server || !Client) return WaitOrFail(TEXT("Listen server and client PIE worlds did not connect."));
            auto* HostDungeon = FindDungeon(Server);
            auto* ClientDungeon = FindDungeon(Client);
            auto* Mode = Server->GetAuthGameMode<AHeavenBreachGameMode>();
            if (!HostDungeon || !ClientDungeon || !Mode) return WaitOrFail(TEXT("Connected worlds are missing dungeon runtime or GameMode."));
            if (HostDungeon->State == EHBDungeonRuntimeState::Failed || ClientDungeon->State == EHBDungeonRuntimeState::Failed)
            {
                Test->AddError(FString::Printf(TEXT("Network dungeon preparation failed: host=%s client=%s"),
                    *HostDungeon->Status, *ClientDungeon->Status));
                return true;
            }
            APlayerController* RemotePlayer = nullptr;
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (It->IsValid() && !It->Get()->IsLocalController()) RemotePlayer = It->Get();
            if (!HostDungeon->IsReady() || !ClientDungeon->IsReady() || !RemotePlayer || !Mode->IsPlayerDungeonLoaded(RemotePlayer))
                return WaitOrFail(FString::Printf(TEXT("Dungeon/real client loading acknowledgement timed out: host=%s client=%s"),
                    *HostDungeon->Status, *ClientDungeon->Status));

            if (bClearing)
            {
                for (const auto& Actor : ClientActors)
                    if (Actor.IsValid() && !Actor->IsActorBeingDestroyed())
                        return WaitOrFail(TEXT("Server generation cleanup did not remove replicated enemies/chests on the client."));
                Test->TestTrue(TEXT("Replicated enemy actors were verified"), VerifiedEnemies > 0);
                Test->TestTrue(TEXT("Replicated chest actors were verified"), VerifiedChests > 0);
                Test->AddInfo(FString::Printf(TEXT("Listen replication verified %d enemies and %d chests, including client destruction."),
                    VerifiedEnemies, VerifiedChests));
                return true;
            }

            if (PhaseStarted == 0.)
            {
                const auto* HostState = Server->GetGameState<AHeavenBreachGameState>();
                const auto* ClientState = Client->GetGameState<AHeavenBreachGameState>();
                Test->TestNotNull(TEXT("Client has replicated dungeon GameState"), ClientState);
                if (!HostState || !ClientState) return true;
                Test->TestEqual(TEXT("Client received the host generation"), ClientDungeon->GetGenerationId(), HostDungeon->GetGenerationId());
                Test->TestEqual(TEXT("Client received the host seed"), ClientDungeon->ActiveSeed, HostDungeon->ActiveSeed);
                Test->TestEqual(TEXT("Client received all rooms"), ClientDungeon->Rooms.Num(), HostDungeon->Rooms.Num());
                Test->TestEqual(TEXT("Network snapshot contains all rooms"), ClientState->GetDungeonSnapshot().Rooms.Num(), HostState->GetDungeonSnapshot().Rooms.Num());
                Test->TestFalse(TEXT("Fixture pauses match start to isolate room replication"), Mode->IsMatchInProgress());
                for (TActorIterator<AEnemyCharacterBase> It(Server); It; ++It) ServerActors.Add(*It);
                for (TActorIterator<ABoxActor> It(Server); It; ++It) ServerActors.Add(*It);
                PhaseStarted = FPlatformTime::Seconds();
            }
            if (RoomIndex >= HostDungeon->Rooms.Num())
            {
                // Cleanup through PCG, leaving loaded rooms and the connection intact
                // so removal must be delivered by actor destruction replication.
                Server->GetSubsystem<UHBEncounterSubsystem>()->ClearGeneration(HostDungeon->GetGenerationId());
                Server->GetSubsystem<UHBChestSpawnSubsystem>()->ClearGeneration(HostDungeon->GetGenerationId());
                bClearing = true;
                PhaseStarted = FPlatformTime::Seconds();
                return false;
            }
            const int32 RoomId = HostDungeon->Rooms[RoomIndex].Id;
            ULevel* HostLevel = HostDungeon->GetLoadedRoomLevel(RoomId);
            ULevel* ClientLevel = ClientDungeon->GetLoadedRoomLevel(RoomId);
            if (!HostLevel || !ClientLevel) return WaitOrFail(TEXT("Ready dungeon has a missing room instance."));
            TArray<AActor*> RoomActors;
            for (const auto& Actor : ServerActors)
                if (Actor.IsValid() && Actor->GetLevel() == HostLevel) RoomActors.Add(Actor.Get());
            if (RoomActors.IsEmpty()) { ++RoomIndex; return false; }
            if (!bFocused)
            {
                // Floors can be far beyond normal net culling distance. Move the
                // viewer to each room rather than changing production relevancy.
                RemotePlayer->SetViewTarget(RoomActors.Last());
                bFocused = true;
                PhaseStarted = FPlatformTime::Seconds();
                return false;
            }
            UNetDriver* HostDriver = Server->GetNetDriver();
            UNetDriver* ClientDriver = Client->GetNetDriver();
            if (!HostDriver || !ClientDriver || !HostDriver->GetNetGuidCache() || !ClientDriver->GetNetGuidCache())
                return WaitOrFail(TEXT("Connected fixture needs actor NetGUID caches."));
            TArray<AActor*> Copies;
            for (AActor* Actor : RoomActors)
            {
                const FNetworkGUID Guid = HostDriver->GetNetGuidCache()->GetNetGUID(Actor);
                AActor* Copy = Guid.IsValid() ? Cast<AActor>(ClientDriver->GetNetGuidCache()->GetObjectFromNetGUID(Guid, true)) : nullptr;
                if (!IsValid(Copy)) return WaitOrFail(FString::Printf(TEXT("Room %d actor never replicated: %s"), RoomId, *Actor->GetPathName()));
                if (!Copy->GetActorLocation().Equals(Actor->GetActorLocation(), 20.f))
                    return WaitOrFail(FString::Printf(TEXT("Room %d actor transform did not synchronize: %s host=%s client=%s"),
                        RoomId, *Actor->GetName(), *Actor->GetActorLocation().ToString(), *Copy->GetActorLocation().ToString()));
                if (const auto* Enemy = Cast<AEnemyCharacterBase>(Actor); Enemy && Enemy->IsDead() != CastChecked<AEnemyCharacterBase>(Copy)->IsDead())
                    return WaitOrFail(FString::Printf(TEXT("Room %d enemy death state did not replicate."), RoomId));
                if (const auto* Chest = Cast<ABoxActor>(Actor); Chest
                    && (Chest->GetBoxState() != CastChecked<ABoxActor>(Copy)->GetBoxState()
                        || Chest->IsLootGenerated() != CastChecked<ABoxActor>(Copy)->IsLootGenerated()
                        || Chest->GetGrade() != CastChecked<ABoxActor>(Copy)->GetGrade()))
                    return WaitOrFail(FString::Printf(TEXT("Room %d chest state did not replicate."), RoomId));
                Copies.Add(Copy);
            }
            if (!bExercised)
            {
                auto* Encounters = Server->GetSubsystem<UHBEncounterSubsystem>();
                const int32 AliveBefore = Encounters->GetAliveCount(HostDungeon->GetGenerationId(), RoomId, HostLevel);
                AEnemyCharacterBase* MovedEnemy = nullptr;
                for (AActor* Actor : RoomActors)
                {
                    if (auto* Enemy = Cast<AEnemyCharacterBase>(Actor); Enemy && !MovedEnemy)
                    {
                        MovedEnemy = Enemy;
                        Enemy->SetActorLocation(Enemy->GetActorLocation() + FVector(40, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
                        Enemy->BecomeDead();
                        Enemy->ForceNetUpdate();
                    }
                    if (auto* Chest = Cast<ABoxActor>(Actor))
                    {
                        // A controller has no container UI; exercise the existing
                        // authoritative open/loot state without adding UI wiring.
                        Chest->Interact_Implementation(RemotePlayer);
                        Chest->ForceNetUpdate();
                    }
                }
                if (MovedEnemy)
                    Test->TestEqual(TEXT("Server encounter count updates after enemy death"),
                        Encounters->GetAliveCount(HostDungeon->GetGenerationId(), RoomId, HostLevel), AliveBefore - 1);
                bExercised = true;
                return false;
            }
            int32 LocalCount = 0;
            for (AActor* Actor : ClientLevel->Actors)
                if (IsValid(Actor) && (Actor->IsA<AEnemyCharacterBase>() || Actor->IsA<ABoxActor>())) ++LocalCount;
            Test->TestEqual(TEXT("No locally generated duplicate actors in client room"), LocalCount, RoomActors.Num());
            Test->TestTrue(TEXT("Client module matches host module"), ClientDungeon->Rooms[RoomIndex].ModuleMap == HostDungeon->Rooms[RoomIndex].ModuleMap);
            Test->TestTrue(TEXT("Client room transform matches host room"), ClientDungeon->Rooms[RoomIndex].Transform.Equals(HostDungeon->Rooms[RoomIndex].Transform));
            for (int32 Index = 0; Index < RoomActors.Num(); ++Index)
            {
                AActor* Copy = Copies[Index];
                Test->TestFalse(TEXT("Replicated room actor has no client authority"), Copy->HasAuthority());
                Test->TestTrue(TEXT("Replicated room actor belongs to the matching streamed room"), Copy->GetLevel() == ClientLevel);
                Test->TestTrue(TEXT("Replicated actor class matches server selection"), Copy->GetClass() == RoomActors[Index]->GetClass());
                if (auto* Enemy = Cast<AEnemyCharacterBase>(Copy))
                {
                    ++VerifiedEnemies;
                    Test->TestNull(TEXT("Client enemy does not run an AI controller"), Enemy->GetController());
                    Test->TestEqual(TEXT("Enemy life state matches server"), Enemy->IsDead(), CastChecked<AEnemyCharacterBase>(RoomActors[Index])->IsDead());
                }
                if (auto* Chest = Cast<ABoxActor>(Copy))
                {
                    ++VerifiedChests;
                    const auto* HostChest = CastChecked<ABoxActor>(RoomActors[Index]);
                    Test->TestEqual(TEXT("Chest grade matches server"), Chest->GetGrade(), HostChest->GetGrade());
                    Test->TestEqual(TEXT("Chest open state matches server"), Chest->GetBoxState(), HostChest->GetBoxState());
                    Test->TestEqual(TEXT("Chest loot generation state matches server"), Chest->IsLootGenerated(), HostChest->IsLootGenerated());
                }
                ClientActors.Add(Copy);
            }
            ++RoomIndex;
            bFocused = false;
            bExercised = false;
            PhaseStarted = FPlatformTime::Seconds();
            return false;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonListenReplicationTest, "HeavenBreach.PCGModule.Network.ListenRoomReplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonListenReplicationTest::RunTest(const FString&)
{
    const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay");
    if (!FPackageName::DoesPackageExist(Map)) { AddError(TEXT("Dungeon network fixture map is missing.")); return false; }
    AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
    const auto Fixture = MakeShared<FHBReplicationFixture>();
    auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->GetPlayNetMode(Fixture->Mode);
    Settings->GetPlayNumberOfClients(Fixture->Clients);
    Settings->GetRunUnderOneProcess(Fixture->bOneProcess);
    Fixture->bSeparateServer = Settings->bLaunchSeparateServer;
    Fixture->Drivers = GEngine->NetDriverDefinitions;
    Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
    Settings->SetPlayNumberOfClients(2);
    Settings->SetRunUnderOneProcess(true);
    Settings->bLaunchSeparateServer = false;
    for (FNetDriverDefinition& Driver : GEngine->NetDriverDefinitions)
        if (Driver.DefName == NAME_GameNetDriver)
        {
            Driver.DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
            Driver.DriverClassNameFallback = Driver.DriverClassName;
        }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture]()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::Editor || !Context.World()) continue;
            auto* WorldSettings = Context.World()->GetWorldSettings();
            if (!WorldSettings->DefaultGameMode || !WorldSettings->DefaultGameMode->IsChildOf(AHeavenBreachGameMode::StaticClass())
                || !FindDungeon(Context.World())) { AddError(TEXT("Network fixture requires its dungeon and gameplay GameMode.")); return true; }
            Fixture->bConfigured = true;
            break;
        }
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForDungeonReplication(this, Fixture));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Fixture]() { Fixture->Restore(); return true; }));
    return true;
}
#endif
