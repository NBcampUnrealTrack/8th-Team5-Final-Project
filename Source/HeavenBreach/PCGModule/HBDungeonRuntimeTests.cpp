#include "PCGModule/HBDungeonRuntime.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "IpNetDriver.h"
#include "Engine/ChildConnection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBClientLayoutBoundaryTest, "HeavenBreach.PCGModule.Gameplay.ClientLayoutBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBClientLayoutBoundaryTest::RunTest(const FString&)
{
    FString Error;
    TArray<FHBPrototypeRoom> Rooms;
    TestFalse(TEXT("Empty layout rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    FHBPrototypeRoom Room;
    Room.Id = 0; Room.ModuleId = TEXT("TestModule");
    Room.ModuleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
    Rooms.Add(Room);
    TestTrue(TEXT("Minimal authoritative layout accepted without catalog/rules"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0].Id = 2;
    TestFalse(TEXT("Non-contiguous ID rejected before stream indexing"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].ParentId = 0;
    TestFalse(TEXT("Self parent rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].UsedConnectorIds.Add(TEXT("Door")); Rooms[0].UnusedConnectorIds.Add(TEXT("Door"));
    TestFalse(TEXT("Conflicting seal intent rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));

    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    TestTrue(TEXT("Standalone may plan"), Dungeon->CanPlanGeneration());
    TestFalse(TEXT("Server does not accept client layout entry point"), Dungeon->ApplyServerLayout(1, 1, Rooms));
    auto* SnapshotState = World->SpawnActor<AHeavenBreachGameState>();
    World->SetGameState(SnapshotState);
    TestFalse(TEXT("Malformed snapshot is not published"), SnapshotState->PublishDungeonLayout(1, 42, Rooms));
    Rooms[0] = Room;
    TestTrue(TEXT("Authority publishes validated snapshot"), SnapshotState->PublishDungeonLayout(1, 42, Rooms));
    Rooms[0].ModuleId = TEXT("ChangedAfterPublication");
    TestEqual(TEXT("Snapshot owns its room data"), SnapshotState->GetDungeonSnapshot().Rooms[0].ModuleId, Room.ModuleId);
    TestFalse(TEXT("Published generation is immutable"), SnapshotState->PublishDungeonLayout(1, 99, Rooms));
    TestFalse(TEXT("Old generation rejected"), SnapshotState->PublishDungeonLayout(0, 99, Rooms));
    auto* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    // A child connection is a socket-free net-mode marker. An uninitialized
    // UIpConnection cannot be safely collected because its socket cleanup needs a driver.
    Driver->ServerConnection = NewObject<UChildConnection>(Driver);
    World->SetNetDriver(Driver);
    TestEqual(TEXT("Fixture is a client world"), World->GetNetMode(), NM_Client);
    TestFalse(TEXT("Local actor authority cannot bypass client planning guard"), Dungeon->CanPlanGeneration());
    Dungeon->StartGeneration();
    TestEqual(TEXT("Client waits without failing or planning"), Dungeon->State, EHBDungeonRuntimeState::Idle);
    TestFalse(TEXT("Client Plan node cannot generate"), Dungeon->PlanLayout());
    Dungeon->MaterializeLayout(1);
    TestEqual(TEXT("Client Load node cannot bypass layout application"), Dungeon->State, EHBDungeonRuntimeState::Idle);
    TestFalse(TEXT("Invalid generation rejected"), Dungeon->ApplyServerLayout(0, 1, Rooms));
    TestFalse(TEXT("Client cannot publish even with local actor authority"), SnapshotState->PublishDungeonLayout(2, 99, Rooms));
    Dungeon->Tick(0);
    TestEqual(TEXT("Snapshot received before runtime binding is consumed"), Dungeon->GetGenerationId(), 1);
    TestEqual(TEXT("Authoritative seed applied"), Dungeon->ActiveSeed, 42);
    TestEqual(TEXT("Local loading starts from retained snapshot"), Dungeon->State, EHBDungeonRuntimeState::Loading);
    SnapshotState->OnDungeonSnapshotChanged.Broadcast();
    TestEqual(TEXT("Repeated notification does not restart generation"), Dungeon->GetGenerationId(), 1);
    World->SetNetDriver(nullptr);
    Driver->ServerConnection = nullptr;
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGameplayGateTest, "HeavenBreach.PCGModule.Gameplay.SpawnGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBGameplayGateTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Mode = World->SpawnActor<AHeavenBreachGameMode>();
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    auto* PC = World->SpawnActor<APlayerController>();
    World->SpawnActor<APlayerStart>(); // Persistent-level fallback must never be used.
    for (const auto State : {EHBDungeonRuntimeState::Idle, EHBDungeonRuntimeState::Generating,
        EHBDungeonRuntimeState::Loading, EHBDungeonRuntimeState::Failed})
    {
        Dungeon->State = State;
        TestNull(TEXT("No start exposed until dungeon ready"), Mode->ChoosePlayerStart_Implementation(PC));
        Mode->RestartPlayer(PC);
        TestNull(TEXT("No premature gameplay pawn"), PC->GetPawn());
        Mode->StartMatch();
        TestFalse(TEXT("Direct StartMatch cannot bypass readiness"), Mode->IsMatchInProgress());
    }
    TestFalse(TEXT("Runtime generator does not inherit preview"), Dungeon->IsA<AHBPCGPreviewGenerator>());
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

class FHBWaitForGameplayDungeon : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
public:
    explicit FHBWaitForGameplayDungeon(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
        if (!World)
        {
            if (FPlatformTime::Seconds() - Started < 90) return false;
            Test->AddError(TEXT("PIE world did not start.")); return true;
        }
        AHBDungeonRuntime* Dungeon = nullptr;
        for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) { Dungeon = *It; break; }
        if (!Dungeon) { Test->AddError(TEXT("Gameplay fixture is missing runtime generator.")); return true; }
        // Supply the same validated starting loadout the lobby would pass in.
        // This isolates PCG from the existing direct-PIE missing-loadout fallback.
        if (!Dungeon->IsReady())
        {
            auto* PC = World->GetFirstPlayerController();
            auto* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
            const auto* GM = World->GetAuthGameMode();
            const auto* CDO = GM && GM->DefaultPawnClass ? Cast<AHBPlayerCharacter>(GM->DefaultPawnClass->GetDefaultObject()) : nullptr;
            if (PS && !PS->GetSelectedJob() && CDO && CDO->GetJobData())
            {
                FHBStartingLoadout Loadout; FText Error;
                if (!MakeDefaultStartingLoadout(CDO->GetJobData(), Loadout, Error) || !PS->TrySetStartingLoadout(Loadout, Error))
                    Test->AddError(Error.ToString());
            }
        }
        if (Dungeon->State == EHBDungeonRuntimeState::Failed) { Test->AddError(Dungeon->Status); return true; }
        if (!Dungeon->IsReady())
        {
            if (FPlatformTime::Seconds() - Started < 90) return false;
            Test->AddError(TEXT("Gameplay dungeon never became ready.")); return true;
        }
        auto* Mode = Cast<AHeavenBreachGameMode>(World->GetAuthGameMode());
        Test->TestNotNull(TEXT("Main game mode is used"), Mode);
        if (!Mode) return true;
        Test->TestTrue(TEXT("Single player starts match after dungeon ready"), Mode->IsMatchInProgress());
        auto* PC = World->GetFirstPlayerController();
        auto* Pawn = PC ? Cast<AHBPlayerCharacter>(PC->GetPawn()) : nullptr;
        Test->TestNotNull(TEXT("Real HB player character is possessed"), Pawn);
        Test->TestNotNull(TEXT("Start point belongs to generated start room"), Dungeon->GetPlayerStart());
        if (Pawn && Dungeon->GetPlayerStart())
            Test->TestTrue(TEXT("Character spawned at authored start"), Pawn->GetActorLocation().Equals(Dungeon->GetPlayerStart()->GetActorLocation(), 10));
        int32 ExpectedSeals = 0, ActualSeals = 0;
        for (const auto& Room : Dungeon->Rooms) ExpectedSeals += Room.UnusedConnectorIds.Num();
        for (TActorIterator<AHBDungeonDoorSeal> It(World); It; ++It) ++ActualSeals;
        Test->TestEqual(TEXT("Unused connectors sealed"), ActualSeals, ExpectedSeals);
        for (TActorIterator<AHBPCGExplorerPawn> It(World); It; ++It) Test->AddError(TEXT("Preview pawn leaked into gameplay."));
        auto* State = World->GetGameState<AHeavenBreachGameState>();
        Test->TestTrue(TEXT("Match timer starts after preparation"), State && State->GetRemainingMatchTime() > 0);
        AHBDungeonPortal* Source = nullptr;
        for (TActorIterator<AHBDungeonPortal> It(World); It; ++It)
            if (IsValid(It->TargetPortal)) { Source = *It; break; }
        Test->TestNotNull(TEXT("Runtime portals linked"), Source);
        if (Pawn && Source)
        {
            Pawn->SetActorLocation(Source->Trigger->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            const FTransform Arrival = Source->TargetPortal->Arrival->GetComponentTransform();
            if (World->EncroachingBlockingGeometry(Pawn, Arrival.GetLocation(), Arrival.Rotator()))
                Test->AddInfo(FString::Printf(TEXT("Portal arrival is blocked for the gameplay pawn: %s at %s"),
                    *Source->TargetPortal->GetPathName(), *Arrival.GetLocation().ToString()));
            Test->TestTrue(TEXT("Real gameplay character can traverse portal"), Source->TryTeleport(Pawn));
            Test->TestTrue(TEXT("Character reached authored arrival"), Pawn->GetActorLocation().Equals(Source->TargetPortal->Arrival->GetComponentLocation(), 1));
        }
        return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGameplaySmokeTest, "HeavenBreach.PCGModule.Gameplay.StreamSpawnAndPortal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBGameplaySmokeTest::RunTest(const FString&)
{
    const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay");
    if (!FPackageName::DoesPackageExist(Map))
    { AddError(TEXT("Create gameplay smoke fixture with Tools/PCGPrototype/author_gameplay_smoke.py.")); return false; }
    // Existing project input asset has a null trigger; unrelated to PCG. Keep the
    // exact known diagnostic visible/expected without masking dungeon errors.
    AddExpectedError(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    EPlayNetMode PreviousMode;
    int32 PreviousClients;
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
    // Headless test uses one standalone PIE client, never an external server.
    GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
    GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForGameplayDungeon(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients]()
    {
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
        return true;
    }));
    return true;
}
#endif
