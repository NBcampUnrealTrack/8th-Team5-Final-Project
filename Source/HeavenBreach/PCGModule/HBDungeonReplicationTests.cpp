#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBStartingLoadout.h"
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

    bool SupplyFixtureLoadouts(FAutomationTestBase* Test, AHeavenBreachGameMode* Mode)
    {
        // Direct PIE bypasses the lobby. Supply its validated default loadout
        // through existing APIs before either gameplay pawn is spawned.
        const auto* Defaults = Mode->DefaultPawnClass
            ? Cast<AHBPlayerCharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
        if (!Defaults || !Defaults->GetJobData())
        { Test->AddError(TEXT("Network fixture needs a default gameplay job.")); return false; }
        for (FConstPlayerControllerIterator It = Mode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            auto* State = It->IsValid() ? It->Get()->GetPlayerState<AHB_PlayerState>() : nullptr;
            FHBStartingLoadout Loadout;
            FText Error;
            if (!State || !MakeDefaultStartingLoadout(Defaults->GetJobData(), Loadout, Error)
                || !State->TrySetStartingLoadout(Loadout, Error))
            { Test->AddError(FString::Printf(TEXT("Network fixture loadout failed: %s"), *Error.ToString())); return false; }
        }
        return true;
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
                Test->TestFalse(TEXT("Room preparation alone does not activate dungeon AI"), HostDungeon->AreEncountersActive());
                FString ActivationError;
                Test->TestFalse(TEXT("Client runtime cannot activate server encounters"), ClientDungeon->ActivateEncounters(ActivationError));
                for (TActorIterator<AEnemyCharacterBase> It(Server); It; ++It)
                    Test->TestNull(TEXT("Pending replicated enemy has no server AI controller"), It->GetController());
                // Movement/death/loot checks belong to gameplay, after deferred
                // possession has initialized the existing enemy implementation.
                if (!SupplyFixtureLoadouts(Test, Mode)) return true;
                Mode->StartMatch();
                Test->TestTrue(TEXT("Replication state changes are exercised after AI activation"), HostDungeon->AreEncountersActive());
                if (!HostDungeon->IsReady()) { Test->AddError(HostDungeon->Status); return true; }
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
                AActor* ViewTarget = nullptr;
                for (AActor* Actor : HostLevel->Actors)
                    if (IsValid(Actor) && Actor->IsA<AHBDungeonModuleDefinition>()) { ViewTarget = Actor; break; }
                if (!ViewTarget) { Test->AddError(TEXT("Replication fixture room has no stable camera target.")); return true; }
                // Use a room actor already loaded on both peers. A newly spawned
                // enemy may itself still be waiting for distance-based relevancy.
                RemotePlayer->SetViewTarget(ViewTarget);
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

    class FHBWaitForDungeonActivation : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FHBReplicationFixture> Fixture;
        double Started = FPlatformTime::Seconds();
        bool bReleased = false;
        TWeakObjectPtr<APlayerController> HeldClient;

    public:
        FHBWaitForDungeonActivation(FAutomationTestBase* InTest, TSharedRef<FHBReplicationFixture> InFixture)
            : Test(InTest), Fixture(InFixture) {}

        virtual bool Update() override
        {
            if (!Fixture->bConfigured) return true;
            if (FPlatformTime::Seconds() - Started > 90.)
            { Test->AddError(TEXT("Listen dungeon activation timed out.")); return true; }
            UWorld* Server = nullptr;
            UWorld* Client = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
                if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
                if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
            }
            auto* Mode = Server ? Server->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
            if (Mode)
                FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, false);
            if (!Server || !Client || !Mode) return false;
            if (!bReleased && !HeldClient.IsValid())
                if (APlayerController* Player = Client->GetFirstPlayerController())
                {
                    // Hold the real loading-report RPC while local room streaming
                    // continues. No player-controller implementation is changed.
                    Player->SetActorTickEnabled(false);
                    HeldClient = Player;
                }
            auto* HostDungeon = FindDungeon(Server);
            auto* ClientDungeon = FindDungeon(Client);
            if (!HostDungeon || !ClientDungeon) return false;
            if (HostDungeon->State == EHBDungeonRuntimeState::Failed || ClientDungeon->State == EHBDungeonRuntimeState::Failed)
            {
                Test->AddError(FString::Printf(TEXT("Activation fixture preparation failed: host=%s client=%s"),
                    *HostDungeon->Status, *ClientDungeon->Status));
                return true;
            }
            if (!HostDungeon->IsReady() || !ClientDungeon->IsReady() || !HeldClient.IsValid()) return false;
            APlayerController* Remote = nullptr;
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (It->IsValid() && !It->Get()->IsLocalController()) Remote = It->Get();
            if (!Remote) return false;
            FString Error;
            if (!bReleased)
            {
                Test->TestFalse(TEXT("Ready client has not sent its held loading report"), Mode->IsPlayerDungeonLoaded(Remote));
                Mode->StartMatch();
                Test->TestFalse(TEXT("Manual match start waits for the real client acknowledgement"), Mode->IsMatchInProgress());
                Test->TestFalse(TEXT("AI activation cannot bypass missing client acknowledgement"), HostDungeon->ActivateEncounters(Error));
                int32 Pending = 0;
                for (TActorIterator<AEnemyCharacterBase> It(Server); It; ++It)
                {
                    ++Pending;
                    Test->TestNull(TEXT("No AI controller exists before all players are loaded"), It->GetController());
                }
                Test->TestTrue(TEXT("Real pending enemies were prepared"), Pending > 0);
                HeldClient->SetActorTickEnabled(true);
                bReleased = true;
                Started = FPlatformTime::Seconds();
                return false;
            }
            if (!Mode->AreDungeonPlayersLoaded()) return false;
            if (!SupplyFixtureLoadouts(Test, Mode)) return true;
            Test->TestFalse(TEXT("Acknowledgement alone does not activate enemies before match start"), HostDungeon->AreEncountersActive());
            Test->TestFalse(TEXT("Runtime activation requires an active match"), HostDungeon->ActivateEncounters(Error));
            Mode->StartMatch();
            Test->TestTrue(TEXT("Acknowledged players can start the match"), Mode->IsMatchInProgress());
            Test->TestTrue(TEXT("GameMode activates encounters when gameplay starts"), HostDungeon->AreEncountersActive());
            if (!HostDungeon->IsReady()) { Test->AddError(HostDungeon->Status); return true; }
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (It->IsValid() && It->Get()->PlayerState && !It->Get()->PlayerState->IsOnlyASpectator())
                    Test->TestNotNull(TEXT("Gameplay players have spawned before AI activation completes"), It->Get()->GetPawn().Get());
            TArray<TWeakObjectPtr<AController>> Controllers;
            auto* Encounters = Server->GetSubsystem<UHBEncounterSubsystem>();
            int32 Active = 0;
            for (TActorIterator<AEnemyCharacterBase> It(Server); It; ++It)
            {
                ++Active;
                AController* Controller = It->GetController();
                Test->TestNotNull(TEXT("Activated server enemy has an AI controller"), Controller);
                if (!Controller) continue;
                Controllers.Add(Controller);
                auto* Tree = Controller->FindComponentByClass<UStateTreeAIComponent>();
                Test->TestTrue(TEXT("Existing enemy StateTree runs after activation"), Tree && Tree->IsRunning());
                Test->TestTrue(TEXT("Enemy ASC initializes at deferred possession"), It->GetEnemyASC() && It->GetEnemyASC()->GetAvatarActor() == *It);
            }
            for (const auto& Room : HostDungeon->Rooms)
            {
                ULevel* Level = HostDungeon->GetLoadedRoomLevel(Room.Id);
                if (UHBEncounterSubsystem::HasEncounter(Level))
                    Test->TestTrue(TEXT("All prepared rooms are marked active for this generation"),
                        Encounters->IsActivatedFor(HostDungeon->GetGenerationId(), Room.Id, Level));
            }
            Test->TestTrue(TEXT("Repeated dungeon activation is idempotent"), HostDungeon->ActivateEncounters(Error));
            int32 Repeated = 0;
            for (TActorIterator<AEnemyCharacterBase> It(Server); It; ++It)
            {
                ++Repeated;
                Test->TestTrue(TEXT("Repeated activation preserves each existing controller"), Controllers.Contains(It->GetController()));
            }
            Test->TestEqual(TEXT("Repeated activation does not spawn more enemies"), Repeated, Active);
            Test->AddInfo(FString::Printf(TEXT("Listen activation verified %d enemies after real client acknowledgement and player spawning."), Active));
            return true;
        }
    };
    class FHBWaitForDungeonGeometry : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FHBReplicationFixture> Fixture;
        double Started = FPlatformTime::Seconds();
        bool bMatchStarted = false;
        bool bTeleported = false;
        bool bChangedClientDefaults = false;
        FVector Destination = FVector::ZeroVector;
        FRotator Facing = FRotator(0, 137, 0);
        int32 SealCount = 0;
        int32 PortalPairs = 0;

        bool CheckGeometry(AHBDungeonRuntime* Host, AHBDungeonRuntime* Client, const FHBDungeonSnapshot& Snapshot)
        {
            for (const auto& Room : Snapshot.Rooms)
            {
                ULevel* HostLevel = Host->GetLoadedRoomLevel(Room.Id);
                ULevel* ClientLevel = Client->GetLoadedRoomLevel(Room.Id);
                if (!HostLevel || !ClientLevel) { Test->AddError(TEXT("Geometry fixture has an unloaded room.")); return false; }
                TArray<AHBDungeonDoorSeal*> HostSeals, ClientSeals;
                for (AActor* Actor : HostLevel->Actors)
                    if (auto* Seal = Cast<AHBDungeonDoorSeal>(Actor); IsValid(Seal) && Seal->GetOwner() == Host) HostSeals.Add(Seal);
                for (AActor* Actor : ClientLevel->Actors)
                    if (auto* Seal = Cast<AHBDungeonDoorSeal>(Actor); IsValid(Seal) && Seal->GetOwner() == Client) ClientSeals.Add(Seal);
                Test->TestEqual(TEXT("Server room has one wall per unused connector"), HostSeals.Num(), Room.UnusedConnectorIds.Num());
                Test->TestEqual(TEXT("Client room has the same number of walls without replicated duplicates"), ClientSeals.Num(), HostSeals.Num());
                for (AHBDungeonDoorSeal* HostSeal : HostSeals)
                {
                    ++SealCount;
                    auto* Found = ClientSeals.FindByPredicate([HostSeal](const auto* Seal) { return Seal->GetFName() == HostSeal->GetFName(); });
                    if (!Found) { Test->AddError(TEXT("Client wall is missing its stable generation/room/connector identity.")); return false; }
                    AHBDungeonDoorSeal* ClientSeal = *Found;
                    Test->TestTrue(TEXT("Both walls use the authoritative class"),
                        HostSeal->GetClass() == Snapshot.DoorSealClass.Get() && ClientSeal->GetClass() == HostSeal->GetClass());
                    Test->TestTrue(TEXT("Wall transforms agree across server and client"), HostSeal->GetActorTransform().Equals(ClientSeal->GetActorTransform(), 0.1));
                    Test->TestTrue(TEXT("Wall collision bounds agree across server and client"),
                        HostSeal->GetComponentsBoundingBox(true).Equals(ClientSeal->GetComponentsBoundingBox(true), 0.1));
                    for (AHBDungeonDoorSeal* Seal : {HostSeal, ClientSeal})
                    {
                        Test->TestFalse(TEXT("Locally generated walls are not replicated actors"), Seal->GetIsReplicated());
                        Test->TestTrue(TEXT("Wall blocks player collision"), Seal->Wall->IsQueryCollisionEnabled()
                            && Seal->Wall->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
                        Test->TestTrue(TEXT("Wall mesh size follows the server snapshot"),
                            (Seal->Wall->GetStaticMesh()->GetBounds().BoxExtent * Seal->Wall->GetRelativeScale3D()).Equals(Snapshot.DoorSealExtent, 0.1));
                    }
                }
                if (Room.Connection != EHBDungeonConnection::Portal) continue;
                ++PortalPairs;
                FString Error;
                auto* HostSource = AHBDungeonPortal::FindInLevel(Host->GetLoadedRoomLevel(Room.ParentId), Room.ParentPortalId, Error);
                auto* HostTarget = AHBDungeonPortal::FindInLevel(HostLevel, Room.EntryPortalId, Error);
                auto* ClientSource = AHBDungeonPortal::FindInLevel(Client->GetLoadedRoomLevel(Room.ParentId), Room.ParentPortalId, Error);
                auto* ClientTarget = AHBDungeonPortal::FindInLevel(ClientLevel, Room.EntryPortalId, Error);
                if (!HostSource || !HostTarget || !ClientSource || !ClientTarget) { Test->AddError(Error); return false; }
                Test->TestTrue(TEXT("Server portal endpoints are reciprocally linked in their planned instances"),
                    HostSource->TargetPortal == HostTarget && HostTarget->TargetPortal == HostSource);
                Test->TestTrue(TEXT("Client portals have matching trigger and arrival geometry"),
                    HostSource->Trigger->GetComponentTransform().Equals(ClientSource->Trigger->GetComponentTransform(), 0.1)
                    && HostTarget->Arrival->GetComponentTransform().Equals(ClientTarget->Arrival->GetComponentTransform(), 0.1));
                Test->TestFalse(TEXT("Locally authoritative client portals cannot connect"), ClientSource->ConnectTo(ClientTarget));
                Test->TestTrue(TEXT("Portal links remain server-only"), !ClientSource->TargetPortal && !ClientTarget->TargetPortal);
            }
            Test->TestTrue(TEXT("Geometry fixture exercised actual unused-door walls"), SealCount > 0);
            Test->TestTrue(TEXT("Geometry fixture exercised actual cross-room portal pairs"), PortalPairs > 0);
            return true;
        }

    public:
        FHBWaitForDungeonGeometry(FAutomationTestBase* InTest, TSharedRef<FHBReplicationFixture> InFixture)
            : Test(InTest), Fixture(InFixture) {}

        virtual bool Update() override
        {
            if (!Fixture->bConfigured) return true;
            if (FPlatformTime::Seconds() - Started > 90.)
            { Test->AddError(TEXT("Listen walls/portal synchronization timed out.")); return true; }
            UWorld* Server = nullptr;
            UWorld* Client = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
                if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
                if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
            }
            auto* Mode = Server ? Server->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
            if (Mode) FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, false);
            auto* HostDungeon = FindDungeon(Server);
            auto* ClientDungeon = FindDungeon(Client);
            if (!Mode || !HostDungeon || !ClientDungeon) return false;
            if (!bChangedClientDefaults)
            {
                ClientDungeon->DoorSealClass = nullptr;
                ClientDungeon->DoorSealExtent = FVector::ZeroVector;
                bChangedClientDefaults = true;
            }
            if (HostDungeon->State == EHBDungeonRuntimeState::Failed || ClientDungeon->State == EHBDungeonRuntimeState::Failed)
            {
                Test->AddError(FString::Printf(TEXT("Geometry fixture failed: host=%s client=%s"), *HostDungeon->Status, *ClientDungeon->Status));
                return true;
            }
            if (!HostDungeon->IsReady() || !ClientDungeon->IsReady() || !Mode->AreDungeonPlayersLoaded()) return false;
            if (!bMatchStarted)
            {
                const auto* HostState = Server->GetGameState<AHeavenBreachGameState>();
                const auto* ClientState = Client->GetGameState<AHeavenBreachGameState>();
                if (!HostState || !ClientState) return false;
                Test->TestTrue(TEXT("Wall asset path and size replicate with the layout"),
                    HostState->GetDungeonSnapshot().DoorSealClass == ClientState->GetDungeonSnapshot().DoorSealClass
                    && HostState->GetDungeonSnapshot().DoorSealExtent.Equals(ClientState->GetDungeonSnapshot().DoorSealExtent));
                if (!CheckGeometry(HostDungeon, ClientDungeon, HostState->GetDungeonSnapshot()) || !SupplyFixtureLoadouts(Test, Mode)) return true;
                Mode->StartMatch();
                Test->TestTrue(TEXT("Portal network test starts only after dungeon acknowledgements"), Mode->IsMatchInProgress());
                if (!Mode->IsMatchInProgress()) return true;
                bMatchStarted = true;
                return false;
            }
            APlayerController* Remote = nullptr;
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (It->IsValid() && !It->Get()->IsLocalController()) Remote = It->Get();
            APlayerController* Local = Client->GetFirstPlayerController();
            auto* HostPawn = Remote ? Cast<ACharacter>(Remote->GetPawn()) : nullptr;
            auto* ClientPawn = Local ? Cast<ACharacter>(Local->GetPawn()) : nullptr;
            if (!HostPawn || !ClientPawn) return false;
            if (!bTeleported)
            {
                // Safe synthetic arrivals isolate networking from authored map clearance.
                HostPawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
                ClientPawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
                HostPawn->GetCharacterMovement()->StopMovementImmediately();
                ClientPawn->GetCharacterMovement()->StopMovementImmediately();
                auto* Source = Server->SpawnActor<AHBDungeonPortal>(HostPawn->GetActorLocation(), FRotator::ZeroRotator);
                auto* Target = Server->SpawnActor<AHBDungeonPortal>(HostPawn->GetActorLocation() + FVector(10000, 5000, 20000), Facing);
                if (!Source || !Target) { Test->AddError(TEXT("Could not spawn safe network portal fixture.")); return true; }
                Source->PortalId = TEXT("NetworkExit"); Target->PortalId = TEXT("NetworkEntry");
                Source->SetActorTickEnabled(false); Target->SetActorTickEnabled(false);
                Destination = Target->Arrival->GetComponentLocation();
                if (!Source->ConnectTo(Target) || !Source->TryTeleport(HostPawn))
                { Test->AddError(TEXT("Server could not traverse the safe network portal fixture.")); return true; }
                Test->TestTrue(TEXT("Server pawn reaches the portal arrival"), HostPawn->GetActorLocation().Equals(Destination, 0.1));
                bTeleported = true;
                return false;
            }
            if (!ClientPawn->GetActorLocation().Equals(Destination, 20.) || !Local->GetControlRotation().Equals(Facing, 1.)) return false;
            Test->TestTrue(TEXT("Owning client receives the portal arrival facing"), Local->GetControlRotation().Equals(Facing, 1.));
            Test->AddInfo(FString::Printf(TEXT("Listen geometry verified %d matching walls, %d portal pairs and remote player position/facing."), SealCount, PortalPairs));
            return true;
        }
    };
    enum class EHBDungeonNetworkTest { Replication, Activation, Geometry };
}

static bool QueueDungeonNetworkTest(FAutomationTestBase* Test, EHBDungeonNetworkTest Kind)
{
    const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay");
    if (!FPackageName::DoesPackageExist(Map)) { Test->AddError(TEXT("Dungeon network fixture map is missing.")); return false; }
    Test->AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    Test->AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
    Test->AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
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
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Fixture]()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::Editor || !Context.World()) continue;
            auto* WorldSettings = Context.World()->GetWorldSettings();
            if (!WorldSettings->DefaultGameMode || !WorldSettings->DefaultGameMode->IsChildOf(AHeavenBreachGameMode::StaticClass())
                || !FindDungeon(Context.World())) { Test->AddError(TEXT("Network fixture requires its dungeon and gameplay GameMode.")); return true; }
            Fixture->bConfigured = true;
            break;
        }
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    if (Kind == EHBDungeonNetworkTest::Geometry)
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBWaitForDungeonGeometry>(Test, Fixture));
    else if (Kind == EHBDungeonNetworkTest::Activation)
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBWaitForDungeonActivation>(Test, Fixture));
    else
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBWaitForDungeonReplication>(Test, Fixture));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Fixture]() { Fixture->Restore(); return true; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonListenReplicationTest, "HeavenBreach.PCGModule.Network.ListenRoomReplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonListenReplicationTest::RunTest(const FString&) { return QueueDungeonNetworkTest(this, EHBDungeonNetworkTest::Replication); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonListenActivationTest, "HeavenBreach.PCGModule.Network.ListenEncounterActivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonListenActivationTest::RunTest(const FString&) { return QueueDungeonNetworkTest(this, EHBDungeonNetworkTest::Activation); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonListenGeometryTest, "HeavenBreach.PCGModule.Network.ListenWallsAndPortals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonListenGeometryTest::RunTest(const FString&) { return QueueDungeonNetworkTest(this, EHBDungeonNetworkTest::Geometry); }
#endif
