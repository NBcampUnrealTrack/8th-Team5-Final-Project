#include "PCGModule/HBDungeonRuntime.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestPlacementComponent.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "AI/EnemyCharacterBase.h"
#include "Loot/BoxActor.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "IpNetDriver.h"
#include "Engine/ChildConnection.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGeneratorRoleLimitsTest, "HeavenBreach.PCGModule.Gameplay.GeneratorRoleLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBGeneratorRoleLimitsTest::RunTest(const FString&)
{
    UClass* GeneratorClass = LoadClass<AHBDungeonRuntime>(nullptr, TEXT("/Game/Core/PCG/BP_HBDungeonRuntime.BP_HBDungeonRuntime_C"));
    if (!TestNotNull(TEXT("Gameplay generator Blueprint loads with inherited role settings"), GeneratorClass)) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>(GeneratorClass);
    if (!TestNotNull(TEXT("Blueprint generator instance spawns"), Dungeon))
    { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
    struct FSetting
    {
        const TCHAR* Name;
        int32 Default;
        int32 Edited;
        int32 FHBDungeonGenerationRules::* Member;
    };
    const FSetting Settings[] = {
        {TEXT("MaxConsecutiveAdventureRooms"), 3, 4, &FHBDungeonGenerationRules::MaxConsecutiveAdventureRooms},
        {TEXT("MaxConsecutiveRecoveryRooms"), 1, 2, &FHBDungeonGenerationRules::MaxConsecutiveRecoveryRooms},
        {TEXT("MaxConsecutiveEnhancementRooms"), 1, 3, &FHBDungeonGenerationRules::MaxConsecutiveEnhancementRooms},
        {TEXT("MaxRecoveryRoomsPerFloor"), 1, 3, &FHBDungeonGenerationRules::MaxRecoveryRoomsPerFloor},
        {TEXT("MaxEnhancementRoomsPerFloor"), 1, 4, &FHBDungeonGenerationRules::MaxEnhancementRoomsPerFloor}
    };
    for (const auto& Setting : Settings)
    {
        auto* Property = FindFProperty<FIntProperty>(GeneratorClass, Setting.Name);
        if (!TestNotNull(FString::Printf(TEXT("BP exposes %s directly"), Setting.Name), Property)) continue;
        TestTrue(TEXT("Setting is editable in both BP Class Defaults and placed instances"), Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
            && !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly | CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate));
        TestEqual(TEXT("Existing Blueprint inherits the original default"), Property->GetPropertyValue_InContainer(GeneratorClass->GetDefaultObject()), Setting.Default);
        Property->SetPropertyValue_InContainer(Dungeon, Setting.Edited);
    }
    const auto Rules = Dungeon->GetGenerationRules();
    for (const auto& Setting : Settings)
        TestEqual(FString::Printf(TEXT("Generation uses BP setting %s"), Setting.Name), Rules.*(Setting.Member), Setting.Edited);
    TestEqual(TEXT("Combining role settings retains authored layout counts"), Rules.Floor1MainRooms, Dungeon->DungeonRules.Floor1MainRooms);
    TestTrue(TEXT("Combining role settings retains the selected exit map"), Rules.Floor1ExitMap == Dungeon->DungeonRules.Floor1ExitMap);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBClientLayoutBoundaryTest, "HeavenBreach.PCGModule.Gameplay.ClientLayoutBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBClientLayoutBoundaryTest::RunTest(const FString&)
{
    FString Error;
    TArray<FHBPrototypeRoom> Rooms;
    TestFalse(TEXT("Empty layout rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    FHBPrototypeRoom Room;
    Room.Id = 0; Room.ModuleId = TEXT("TestModule"); Room.bIsStartRoom = true;
    Room.ModuleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
    Rooms.Add(Room);
    TestTrue(TEXT("Minimal authoritative layout accepted without catalog/rules"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0].bIsStartRoom = false;
    TestFalse(TEXT("Snapshot must identify the actual start"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].Role = EHBPrototypeRoomRole::Boss;
    TestFalse(TEXT("Snapshot cannot identify a boss as the start"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room;
    Rooms[0].Id = 2;
    TestFalse(TEXT("Non-contiguous ID rejected before stream indexing"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].ParentId = 0;
    TestFalse(TEXT("Self parent rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].UsedConnectorIds.Add(TEXT("Door")); Rooms[0].UnusedConnectorIds.Add(TEXT("Door"));
    TestFalse(TEXT("Conflicting seal intent rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room; Rooms[0].Connection = EHBDungeonConnection::Portal;
    TestFalse(TEXT("Root portal rejected before parent stream indexing"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[0] = Room;
    FHBPrototypeRoom Child = Room;
    Child.Id = 1; Child.ParentId = 0; Child.Connection = EHBDungeonConnection::Portal; Child.bIsStartRoom = false;
    Child.ParentPortalId = TEXT("Exit"); Child.EntryPortalId = TEXT("Entry");
    Rooms.Add(Child);
    TestTrue(TEXT("Distinct room portal endpoints accepted"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[1].bIsStartRoom = true;
    TestFalse(TEXT("Snapshot cannot contain two actual start instances"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms[1] = Child;
    Child.Id = 2; Rooms.Add(Child);
    TestFalse(TEXT("Reusing a parent portal endpoint rejected"), HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error));
    Rooms.SetNum(1);
    Rooms[0].UsedConnectorIds.Add(TEXT("Door")); Rooms[0].UnusedConnectorIds.Add(TEXT("Door"));

    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    TestTrue(TEXT("Standalone may plan"), Dungeon->CanPlanGeneration());
    TestFalse(TEXT("Server does not accept client layout entry point"), Dungeon->ApplyServerLayout(FHBDungeonSnapshot()));
    auto* SnapshotState = World->SpawnActor<AHeavenBreachGameState>();
    World->SetGameState(SnapshotState);
    TestFalse(TEXT("Malformed snapshot is not published"), SnapshotState->PublishDungeonLayout(1, 42, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    Rooms[0] = Room;
    TestFalse(TEXT("Missing seal class cannot be published"), SnapshotState->PublishDungeonLayout(1, 42, Rooms, nullptr, FVector(30, 160, 220)));
    TestFalse(TEXT("Invalid seal size cannot be published"), SnapshotState->PublishDungeonLayout(1, 42, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector::ZeroVector));
    TestTrue(TEXT("Authority publishes validated snapshot"), SnapshotState->PublishDungeonLayout(1, 42, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    Rooms[0].ModuleId = TEXT("ChangedAfterPublication");
    TestEqual(TEXT("Snapshot owns its room data"), SnapshotState->GetDungeonSnapshot().Rooms[0].ModuleId, Room.ModuleId);
    TestTrue(TEXT("Snapshot retains the server-selected start instance"), SnapshotState->GetDungeonSnapshot().Rooms[0].bIsStartRoom);
    TestFalse(TEXT("Published generation is immutable"), SnapshotState->PublishDungeonLayout(1, 99, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    TestFalse(TEXT("Old generation rejected"), SnapshotState->PublishDungeonLayout(0, 99, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
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
    TestFalse(TEXT("Invalid generation rejected"), Dungeon->ApplyServerLayout(FHBDungeonSnapshot()));
    TestFalse(TEXT("Client cannot publish even with local actor authority"), SnapshotState->PublishDungeonLayout(2, 99, Rooms, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    // A client has no server-side navigation timeout requirement.
    Dungeon->EncounterTimeoutSeconds = 0.f;
    // Clients must use the server's wall settings even when their local defaults differ.
    Dungeon->DoorSealClass = nullptr;
    Dungeon->DoorSealExtent = FVector::ZeroVector;
    Dungeon->Tick(0);
    TestEqual(TEXT("Snapshot received before runtime binding is consumed"), Dungeon->GetGenerationId(), 1);
    TestEqual(TEXT("Authoritative seed applied"), Dungeon->ActiveSeed, 42);
    TestTrue(TEXT("Snapshot retains the server wall class"), SnapshotState->GetDungeonSnapshot().DoorSealClass.Get() == AHBDungeonDoorSeal::StaticClass());
    TestTrue(TEXT("Snapshot retains the server wall size"), SnapshotState->GetDungeonSnapshot().DoorSealExtent.Equals(FVector(30, 160, 220)));
    TestEqual(TEXT("Local loading starts from retained snapshot"), Dungeon->State, EHBDungeonRuntimeState::Loading);
    SnapshotState->OnDungeonSnapshotChanged.Broadcast();
    TestEqual(TEXT("Repeated notification does not restart generation"), Dungeon->GetGenerationId(), 1);
    TestTrue(TEXT("Same client generation does not restart streaming"),
        Dungeon->ApplyServerLayout(SnapshotState->GetDungeonSnapshot()));
    auto* Encounters = World->GetSubsystem<UHBEncounterSubsystem>();
    auto* Chests = World->GetSubsystem<UHBChestSpawnSubsystem>();
    TestNotNull(TEXT("Client encounter subsystem exists"), Encounters);
    TestNotNull(TEXT("Client chest subsystem exists"), Chests);
    if (Encounters) TestFalse(TEXT("Client cannot start room enemies"),
        Encounters->StartEncounter(1, 42, 0, World->PersistentLevel, Room.Bounds, {}, Error));
    if (Chests) TestFalse(TEXT("Client cannot start room chests"),
        Chests->StartPlacement(1, 42, 0, World->PersistentLevel, Room.Bounds, {}, Error));
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        AddError(TEXT("Client snapshot loading created a navigation bounds volume."));
    World->SetNetDriver(nullptr);
    Driver->ServerConnection = nullptr;
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonLoadAckTest, "HeavenBreach.PCGModule.Gameplay.LoadAcknowledgement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonLoadAckTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Mode = World->SpawnActor<AHeavenBreachGameMode>();
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    auto* Source = World->SpawnActor<AHeavenBreachGameState>();
    World->SetGameState(Source);
    auto* Player = World->SpawnActor<APlayerController>();
    World->AddController(Player); // This fixture does not run engine player initialization.
    Player->PlayerState = World->SpawnActor<APlayerState>();
    FHBPrototypeRoom Room;
    Room.Id = 0; Room.ModuleId = TEXT("TestModule"); Room.bIsStartRoom = true;
    Room.ModuleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
    TestTrue(TEXT("Publish generation"), Source->PublishDungeonLayout(1, 42, {Room}, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    TestFalse(TEXT("Unknown controller rejected"), Mode->ReportDungeonLoaded(nullptr, 1, true));
    TestFalse(TEXT("Zero generation rejected"), Mode->ReportDungeonLoaded(Player, 0, true));
    TestFalse(TEXT("Future generation rejected"), Mode->ReportDungeonLoaded(Player, 2, true));
    TestTrue(TEXT("Client acknowledgement accepted before host finishes"), Mode->ReportDungeonLoaded(Player, 1, true));
    TestTrue(TEXT("Duplicate acknowledgement is harmless"), Mode->ReportDungeonLoaded(Player, 1, true));
    TestFalse(TEXT("Host still loading blocks all-player readiness"), Mode->AreDungeonPlayersLoaded());
    Mode->StartMatch(); Mode->RestartPlayer(Player);
    TestFalse(TEXT("Acknowledgement cannot bypass host readiness"), Mode->IsMatchInProgress());
    TestNull(TEXT("Acknowledgement cannot spawn before host is ready"), Player->GetPawn());
    auto* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    Driver->ServerConnection = NewObject<UChildConnection>(Driver);
    World->SetNetDriver(Driver);
    TestTrue(TEXT("Stage the current generation in the local loader"), Dungeon->ApplyServerLayout(Source->GetDungeonSnapshot()));
    Driver->ServerConnection = nullptr;
    TestEqual(TEXT("Fixture becomes a listen server"), World->GetNetMode(), NM_ListenServer);
    TestTrue(TEXT("Listen server can plan while client cannot"), Dungeon->CanPlanGeneration());
    Player->SetAutonomousProxy(true);
    Dungeon->State = EHBDungeonRuntimeState::PreparingEncounters;
    auto* SecondPlayer = World->SpawnActor<APlayerController>();
    World->AddController(SecondPlayer);
    SecondPlayer->PlayerState = World->SpawnActor<APlayerState>();
    SecondPlayer->SetAutonomousProxy(true);
    TestFalse(TEXT("Fixture represents a remote participant"), SecondPlayer->IsLocalController());
    TestFalse(TEXT("One unfinished participant blocks everyone"), Mode->AreDungeonPlayersLoaded());
    TestTrue(TEXT("Second participant completes"), Mode->ReportDungeonLoaded(SecondPlayer, 1, true));
    TestFalse(TEXT("All client reports still wait for server room spawns"), Mode->AreDungeonPlayersLoaded());
    Mode->StartMatch(); Mode->RestartPlayer(Player);
    TestFalse(TEXT("PreparingEncounters blocks a direct match start"), Mode->IsMatchInProgress());
    TestNull(TEXT("PreparingEncounters blocks player spawning"), Player->GetPawn());
    Dungeon->State = EHBDungeonRuntimeState::Ready;
    TestTrue(TEXT("Host and both participants ready opens readiness gate"), Mode->AreDungeonPlayersLoaded());
    TestTrue(TEXT("Publish next generation"), Source->PublishDungeonLayout(2, 43, {Room}, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    TestFalse(TEXT("Previous generation acknowledgement rejected"), Mode->ReportDungeonLoaded(Player, 1, true));
    TestFalse(TEXT("Old acknowledgements do not unlock a new generation"), Mode->AreDungeonPlayersLoaded());
    Player->PlayerState->SetIsOnlyASpectator(true);
    TestFalse(TEXT("Spectator cannot report participant readiness"), Mode->ReportDungeonLoaded(Player, 2, true));
    Player->PlayerState->SetIsOnlyASpectator(false);
    TestTrue(TEXT("Current generation loading can be acknowledged"), Mode->ReportDungeonLoaded(Player, 2, true));
    AddExpectedErrorPlain(TEXT("FAILED: A client failed to prepare the current dungeon generation."), EAutomationExpectedErrorFlags::Contains, 1);
    // The host generation normally advances before it publishes. Stage that identity in this socket-free fixture.
    Dungeon->GenerationId = 2;
    TestFalse(TEXT("Failure after a successful acknowledgement is not ignored"), Mode->ReportDungeonLoaded(Player, 2, false));
    TestEqual(TEXT("Failed acknowledgement invalidates the server snapshot"), Source->GetDungeonSnapshot().State, EHBDungeonSnapshotState::Failed);
    TestFalse(TEXT("Late success cannot resurrect a failed generation"), Mode->ReportDungeonLoaded(Player, 2, true));
    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonLifecycleBoundaryTest, "HeavenBreach.PCGModule.Gameplay.GenerationLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonLifecycleBoundaryTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    auto* Source = World->SpawnActor<AHeavenBreachGameState>();
    World->SetGameState(Source);
    FHBPrototypeRoom Room;
    Room.Id = 0; Room.ModuleId = TEXT("LifecycleModule"); Room.bIsStartRoom = true;
    Room.ModuleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
    TestTrue(TEXT("Publish the first lifecycle generation"), Source->PublishDungeonLayout(1, 42, {Room}, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    const FHBDungeonSnapshot OldLayout = Source->GetDungeonSnapshot();
    auto* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    auto* Connection = NewObject<UChildConnection>(Driver);
    Driver->ServerConnection = Connection;
    World->SetNetDriver(Driver);
    Dungeon->Tick(0.f);
    TestEqual(TEXT("Client started loading"), Dungeon->State, EHBDungeonRuntimeState::Loading);
    TestFalse(TEXT("Client cannot reset a server generation"), Dungeon->ResetGeneration());
    TestFalse(TEXT("Client cannot invalidate the shared snapshot"), Source->InvalidateDungeonLayout(1));

    Driver->ServerConnection = nullptr;
    TestFalse(TEXT("Unrelated cleanup cannot invalidate the current generation"), Source->InvalidateDungeonLayout(0));
    TestTrue(TEXT("Server marks the generation cleared"), Source->InvalidateDungeonLayout(1));
    TestTrue(TEXT("Repeated invalidation is harmless"), Source->InvalidateDungeonLayout(1));
    TestFalse(TEXT("Cleared generation cannot be republished"), Source->PublishDungeonLayout(1, 42, {Room}, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    Driver->ServerConnection = Connection;
    Source->OnDungeonSnapshotChanged.Broadcast();
    TestEqual(TEXT("Client honors a terminal update for the same generation"), Dungeon->State, EHBDungeonRuntimeState::Idle);
    TestTrue(TEXT("Terminal update clears local room data"), Dungeon->Rooms.IsEmpty() && !Dungeon->GetLoadedRoomLevel(0));
    TestFalse(TEXT("Old layout cannot restart a cleared client generation"), Dungeon->ApplyServerLayout(OldLayout));

    Driver->ServerConnection = nullptr;
    TestTrue(TEXT("Pre-layout failure still publishes its generation identity"), Source->InvalidateDungeonLayout(2, TEXT("Lifecycle pre-layout failure")));
    Driver->ServerConnection = Connection;
    Source->OnDungeonSnapshotChanged.Broadcast();
    TestEqual(TEXT("Client receives failure without a published room layout"), Dungeon->State, EHBDungeonRuntimeState::Failed);
    TestEqual(TEXT("Failure advances the client generation"), Dungeon->GetGenerationId(), 2);
    TestEqual(TEXT("Failure reason is retained for late subscribers"), Dungeon->Status, FString(TEXT("Lifecycle pre-layout failure")));
    auto* LateDungeon = World->SpawnActor<AHBDungeonRuntime>();
    LateDungeon->Tick(0.f);
    TestEqual(TEXT("Late subscriber consumes terminal state without loading old rooms"), LateDungeon->State, EHBDungeonRuntimeState::Failed);
    TestTrue(TEXT("Late subscriber has no stale rooms"), LateDungeon->Rooms.IsEmpty());
    LateDungeon->ShutdownGeneration();

    Driver->ServerConnection = nullptr;
    TestTrue(TEXT("New generation replaces terminal state"), Source->PublishDungeonLayout(3, 43, {Room}, AHBDungeonDoorSeal::StaticClass(), FVector(30, 160, 220)));
    Driver->ServerConnection = Connection;
    Source->OnDungeonSnapshotChanged.Broadcast();
    TestEqual(TEXT("Client resumes only for a newer generation"), Dungeon->State, EHBDungeonRuntimeState::Loading);
    TestTrue(TEXT("Runtime is subscribed before shutdown"), Source->OnDungeonSnapshotChanged.IsBoundToObject(Dungeon));
    Dungeon->ShutdownGeneration();
    Dungeon->ShutdownGeneration();
    TestFalse(TEXT("Shutdown removes snapshot subscription"), Source->OnDungeonSnapshotChanged.IsBoundToObject(Dungeon));
    TestTrue(TEXT("Shutdown clears every local room reference"), Dungeon->Rooms.IsEmpty() && !Dungeon->GetLoadedRoomLevel(0));
    FHBDungeonSnapshot NewLayout = OldLayout; NewLayout.GenerationId = 4;
    TestFalse(TEXT("An ended runtime cannot resume from a later snapshot"), Dungeon->ApplyServerLayout(NewLayout));
    Dungeon->Tick(0.f);
    TestFalse(TEXT("An ended runtime cannot resubscribe"), Source->OnDungeonSnapshotChanged.IsBoundToObject(Dungeon));
    World->SetNetDriver(nullptr);
    Driver->ServerConnection = nullptr;
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

namespace
{
	constexpr TCHAR TestMGMapPath[] = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestMG");

	struct FTestMGRuntimeFixtureState
	{
		TArray<TSoftObjectPtr<UWorld>> ModuleMaps;
		int32 Seed = 0;
		bool bRandomSeedOnStart = true;
		bool bConfigured = false;
	};

	AHBDungeonRuntime* FindEditorDungeon()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Editor && Context.World())
				for (TActorIterator<AHBDungeonRuntime> It(Context.World()); It; ++It)
					return *It;
		return nullptr;
	}
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
        EHBDungeonRuntimeState::Loading, EHBDungeonRuntimeState::PreparingEncounters,
        EHBDungeonRuntimeState::Failed})
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

class FHBWaitForTestMGRuntime : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TSharedRef<FTestMGRuntimeFixtureState> Fixture;
	double Started = FPlatformTime::Seconds();
	double ReadyAt = 0;
	int32 InitialAlive = INDEX_NONE;
	int32 InitialChests = INDEX_NONE;

public:
	FHBWaitForTestMGRuntime(FAutomationTestBase* InTest, TSharedRef<FTestMGRuntimeFixtureState> InFixture)
		: Test(InTest), Fixture(InFixture) {}
	virtual bool Update() override
	{
		if (!Fixture->bConfigured) return true;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
		if (!World)
		{
			if (FPlatformTime::Seconds() - Started < 90.) return false;
			Test->AddError(TEXT("TestMG runtime PIE world did not start.")); return true;
		}
		AHBDungeonRuntime* Dungeon = nullptr;
		for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) { Dungeon = *It; break; }
		if (!Dungeon) { Test->AddError(TEXT("TestMG runtime fixture has no dungeon generator.")); return true; }
		if (!Dungeon->IsReady())
		{
			// Direct PIE has no lobby; provide the same default loadout as the existing gameplay smoke test.
			APlayerController* PC = World->GetFirstPlayerController();
			AHB_PlayerState* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
			const AGameModeBase* GM = World->GetAuthGameMode();
			const AHBPlayerCharacter* CDO = GM && GM->DefaultPawnClass
				? Cast<AHBPlayerCharacter>(GM->DefaultPawnClass->GetDefaultObject()) : nullptr;
			if (PS && !PS->GetSelectedJob() && CDO && CDO->GetJobData())
			{
				FHBStartingLoadout Loadout; FText Error;
				if (!MakeDefaultStartingLoadout(CDO->GetJobData(), Loadout, Error)
					|| !PS->TrySetStartingLoadout(Loadout, Error)) Test->AddError(Error.ToString());
			}
		}
		if (Dungeon->State == EHBDungeonRuntimeState::Failed) { Test->AddError(Dungeon->Status); return true; }
		if (!Dungeon->IsReady())
		{
			if (FPlatformTime::Seconds() - Started < 90.) return false;
			Test->AddError(TEXT("TestMG runtime dungeon did not become ready.")); return true;
		}
		FBox PlannedNavigationBounds(ForceInit);
        const auto* SnapshotState = World->GetGameState<AHeavenBreachGameState>();
        Test->TestNotNull(TEXT("Runtime uses the replicated dungeon GameState"), SnapshotState);
        if (SnapshotState)
        {
            const auto& Snapshot = SnapshotState->GetDungeonSnapshot();
            Test->TestEqual(TEXT("Room spawns and snapshot share a generation"), Snapshot.GenerationId, Dungeon->GenerationId);
            Test->TestEqual(TEXT("Room spawns and snapshot share a seed"), Snapshot.Seed, Dungeon->ActiveSeed);
            Test->TestEqual(TEXT("Published room count matches the prepared dungeon"), Snapshot.Rooms.Num(), Dungeon->Rooms.Num());
            for (int32 Index = 0; Index < Snapshot.Rooms.Num() && Index < Dungeon->Rooms.Num(); ++Index)
            {
                Test->TestTrue(TEXT("Published module matches the loaded room"), Snapshot.Rooms[Index].ModuleMap == Dungeon->Rooms[Index].ModuleMap);
                Test->TestTrue(TEXT("Published transform matches the loaded room"), Snapshot.Rooms[Index].Transform.Equals(Dungeon->Rooms[Index].Transform));
            }
        }
		for (const FHBPrototypeRoom& Room : Dungeon->Rooms) PlannedNavigationBounds += Room.Bounds;
		int32 NavigationVolumeCount = 0;
		for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
		{
			++NavigationVolumeCount;
			Test->TestTrue(TEXT("Dungeon navigation volume belongs to the persistent level"),
				It->GetLevel() == World->PersistentLevel);
			const FBox Bounds = It->GetComponentsBoundingBox(true);
			Test->TestTrue(TEXT("One navigation volume covers every generated room"),
				Bounds.IsInsideOrOn(PlannedNavigationBounds.Min)
				&& Bounds.IsInsideOrOn(PlannedNavigationBounds.Max));
		}
		Test->TestEqual(TEXT("Exactly one navigation volume in the generated dungeon"), NavigationVolumeCount, 1);
		if (ReadyAt != 0. && FPlatformTime::Seconds() - ReadyAt < 0.5) return false;

		int32 RoomCount = 0;
		int32 TotalAlive = 0;
		int32 TotalChests = 0;
		UHBEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UHBEncounterSubsystem>();
		UHBChestSpawnSubsystem* ChestSubsystem = World->GetSubsystem<UHBChestSpawnSubsystem>();
		Test->TestNotNull(TEXT("Gameplay world has an encounter subsystem"), EncounterSubsystem);
		Test->TestNotNull(TEXT("Gameplay world has a chest spawn subsystem"), ChestSubsystem);
		if (!EncounterSubsystem || !ChestSubsystem) return true;
		for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
		{
			ULevel* Level = Dungeon->GetLoadedRoomLevel(Room.Id);
			if (!Level || !UHBChestSpawnSubsystem::HasPlacement(Level)) continue;
			AHBChestSetupActor* ChestSetup = nullptr;
			for (AActor* Actor : Level->Actors)
				if (auto* Candidate = Cast<AHBChestSetupActor>(Actor); IsValid(Candidate))
				{ ChestSetup = Candidate; break; }
			Test->TestNotNull(TEXT("Chest room has its setup actor"), ChestSetup);
			if (!ChestSetup || !ChestSetup->Placement) continue;
			Test->TestTrue(TEXT("Chest placement finishes before dungeon Ready"),
				ChestSubsystem->IsPreparedFor(Dungeon->GenerationId, Room.Id, Level));
			int32 Minimum = 0, Maximum = 0;
			for (const FHBChestPlacementGroup& Group : ChestSetup->Placement->Groups)
			{ Minimum += Group.MinCount; Maximum += Group.MaxCount; }
			const int32 Count = ChestSubsystem->GetSpawnedCount(Dungeon->GenerationId, Room.Id, Level);
            const TArray<FVector> RetryEntries{Room.Bounds.GetCenter()};
            FString RetryError;
            Test->TestTrue(TEXT("Repeated chest preparation reuses the same room"),
                ChestSubsystem->StartPlacement(Dungeon->GenerationId, Dungeon->ActiveSeed, Room.Id, Level, Room.Bounds, RetryEntries, RetryError));
            Test->TestFalse(TEXT("Chest generation cannot change seed"),
                ChestSubsystem->StartPlacement(Dungeon->GenerationId, Dungeon->ActiveSeed ^ 1, Room.Id, Level, Room.Bounds, RetryEntries, RetryError));
            Test->TestFalse(TEXT("Same chest room cannot use another level instance"),
                ChestSubsystem->StartPlacement(Dungeon->GenerationId, Dungeon->ActiveSeed, Room.Id, World->PersistentLevel, Room.Bounds, RetryEntries, RetryError));
            Test->TestFalse(TEXT("A loaded chest room cannot join another generation before cleanup"),
                ChestSubsystem->StartPlacement(Dungeon->GenerationId + 1, Dungeon->ActiveSeed, Room.Id, Level, Room.Bounds, RetryEntries, RetryError));
            Test->TestFalse(TEXT("Zero chest generation is rejected"),
                ChestSubsystem->StartPlacement(0, Dungeon->ActiveSeed, Room.Id, Level, Room.Bounds, RetryEntries, RetryError));
            Test->TestFalse(TEXT("Negative chest room ID is rejected"),
                ChestSubsystem->StartPlacement(Dungeon->GenerationId, Dungeon->ActiveSeed, INDEX_NONE, Level, Room.Bounds, RetryEntries, RetryError));
            Test->TestEqual(TEXT("Repeated and rejected chest requests preserve the count"),
                ChestSubsystem->GetSpawnedCount(Dungeon->GenerationId, Room.Id, Level), Count);
			Test->TestTrue(TEXT("Spawned chest count respects the room settings"), Count >= Minimum && Count <= Maximum);
			int32 RoomActorCount = 0;
			for (TActorIterator<ABoxActor> It(World); It; ++It)
				if (It->GetLevel() == Level && It->GetOwner() == ChestSetup) ++RoomActorCount;
			Test->TestEqual(TEXT("Chest subsystem count matches its actors in this room"), Count, RoomActorCount);
			TotalChests += Count;
		}
		for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
		{
			if (Room.ModuleMap.ToSoftObjectPath().GetLongPackageName() != TestMGMapPath) continue;
			++RoomCount;
			ULevel* Level = Dungeon->GetLoadedRoomLevel(Room.Id);
			Test->TestNotNull(TEXT("Generated TestMG room has a loaded level"), Level);
			if (!Level) continue;
			for (AActor* Actor : Level->Actors)
				Test->TestFalse(TEXT("TestMG room has no authored NavMeshBoundsVolume"),
					IsValid(Cast<ANavMeshBoundsVolume>(Actor)));
			AHBEncounterSetupActor* Setup = nullptr;
			for (AActor* Actor : Level->Actors)
				if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor)) { Setup = Candidate; break; }
			Test->TestNotNull(TEXT("Generated TestMG room has an encounter setup"), Setup);
			if (!Setup || !Setup->Encounter) continue;
			Test->TestTrue(TEXT("TestMG encounter prepared before dungeon Ready"),
				EncounterSubsystem->IsPreparedFor(Dungeon->GenerationId, Room.Id, Level));
			int32 Common = 0, Elite = 0;
			for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
			{
				AEnemyCharacterBase* Enemy = *It;
				if (Enemy->GetLevel() != Level) continue;
				auto MatchesGrade = [Setup, Enemy](EHBEncounterGrade Grade)
				{
					return Setup->Encounter->Groups.ContainsByPredicate([Grade, Enemy](const FHBEncounterGroup& Group)
					{
						return Group.Grade == Grade && Group.Monsters.ContainsByPredicate(
							[Enemy](const FHBEncounterMonsterOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); });
					});
				};
				if (MatchesGrade(EHBEncounterGrade::Elite)) ++Elite;
				else if (MatchesGrade(EHBEncounterGrade::Common)) ++Common;
				else Test->AddError(FString::Printf(TEXT("Unexpected TestMG monster: %s"), *Enemy->GetClass()->GetName()));
			}
			Test->TestTrue(TEXT("TestMG has one or two common monsters"), Common >= 1 && Common <= 2);
			Test->TestEqual(TEXT("TestMG has one Stone Golem elite"), Elite, 1);
			Test->TestEqual(TEXT("TestMG server alive count matches spawned monsters"),
				EncounterSubsystem->GetAliveCount(Dungeon->GenerationId, Room.Id, Level), Common + Elite);
			TotalAlive += EncounterSubsystem->GetAliveCount(Dungeon->GenerationId, Room.Id, Level);
		}
		Test->TestTrue(TEXT("Actual dungeon selected TestMG"), RoomCount > 0);
		if (ReadyAt == 0.)
		{ ReadyAt = FPlatformTime::Seconds(); InitialAlive = TotalAlive; InitialChests = TotalChests; return false; }
		if (FPlatformTime::Seconds() - ReadyAt < 0.5) return false;
		Test->TestEqual(TEXT("Ready does not trigger a duplicate TestMG spawn"), TotalAlive, InitialAlive);
		Test->TestEqual(TEXT("Ready does not trigger duplicate room chests"), TotalChests, InitialChests);
        // Simulate a client world without advancing PIE: even locally authoritative
        // actor copies must survive an attempted client-side generation cleanup.
        TArray<TWeakObjectPtr<AActor>> ManagedActors;
        for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It) ManagedActors.Add(*It);
        for (TActorIterator<ABoxActor> It(World); It; ++It) ManagedActors.Add(*It);
        auto* PreviousDriver = World->GetNetDriver();
        auto* ClientDriver = NewObject<UIpNetDriver>(World);
        ClientDriver->SetWorld(World);
        ClientDriver->ServerConnection = NewObject<UChildConnection>(ClientDriver);
        World->SetNetDriver(ClientDriver);
        EncounterSubsystem->ClearGeneration(Dungeon->GenerationId);
        ChestSubsystem->ClearGeneration(Dungeon->GenerationId);
        for (const auto& Actor : ManagedActors)
            Test->TestTrue(TEXT("Client cleanup preserves server-managed actors"), Actor.IsValid() && !Actor->IsActorBeingDestroyed());
        World->SetNetDriver(PreviousDriver);
        ClientDriver->ServerConnection = nullptr;
        ClientDriver->SetWorld(nullptr);
        EncounterSubsystem->ClearGeneration(Dungeon->GenerationId + 1);
        ChestSubsystem->ClearGeneration(Dungeon->GenerationId + 1);
        for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
        {
            ULevel* Level = Dungeon->GetLoadedRoomLevel(Room.Id);
            if (UHBEncounterSubsystem::HasEncounter(Level)) Test->TestTrue(TEXT("Unrelated cleanup preserves encounter preparation"),
                EncounterSubsystem->IsPreparedFor(Dungeon->GenerationId, Room.Id, Level));
            if (UHBChestSpawnSubsystem::HasPlacement(Level)) Test->TestTrue(TEXT("Unrelated cleanup preserves chest preparation"),
                ChestSubsystem->IsPreparedFor(Dungeon->GenerationId, Room.Id, Level));
        }
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGRuntimeTest, "HeavenBreach.PCGModule.Encounter.TestMGRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGRuntimeTest::RunTest(const FString&)
{
	const FString FixtureMap = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay");
	if (!FPackageName::DoesPackageExist(FixtureMap) || !FPackageName::DoesPackageExist(TestMGMapPath))
	{ AddError(TEXT("TestMG or gameplay fixture map is missing.")); return false; }
	AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	const TSharedRef<FTestMGRuntimeFixtureState> Saved = MakeShared<FTestMGRuntimeFixtureState>();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(FixtureMap));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Saved]()
	{
		AHBDungeonRuntime* Dungeon = FindEditorDungeon();
		if (!Dungeon) { AddError(TEXT("Gameplay fixture has no editor dungeon generator.")); return true; }
		Saved->ModuleMaps = Dungeon->ModuleMaps;
		Saved->Seed = Dungeon->Seed;
		Saved->bRandomSeedOnStart = Dungeon->bRandomSeedOnStart;
		Dungeon->ModuleMaps.Add(TSoftObjectPtr<UWorld>(FSoftObjectPath(FString(TestMGMapPath) + TEXT(".TestMG"))));
		TArray<FHBModuleCatalogEntry> Catalog;
		TArray<FString> CatalogErrors;
		if (!HBDungeonModuleCatalog::Build(Dungeon->ModuleMaps, Catalog, CatalogErrors))
		{
			AddError(FString::Join(CatalogErrors, TEXT("\n")));
			return true;
		}
		HBConnectorLayout::FSettings Settings;
		Settings.Origin = Dungeon->GetActorTransform();
		Settings.bAllowModuleReuse = Dungeon->bAllowModuleReuse;
		Settings.MaxSearchSteps = Dungeon->MaxConnectionAttempts;
		Settings.MaxSearchSeconds = Dungeon->ConnectionSearchSeconds;
		int32 SelectedSeed = INDEX_NONE;
		for (int32 Candidate = 1; Candidate <= 64; ++Candidate)
		{
			Settings.Seed = Candidate;
			TArray<FHBPrototypeRoom> Rooms;
			FString Error;
			if (HBDungeonRuleLayout::Generate(Catalog, Settings, Dungeon->GetGenerationRules(), Rooms, Error)
				&& Rooms.ContainsByPredicate([](const FHBPrototypeRoom& Room)
				{
					return Room.ModuleMap.ToSoftObjectPath().GetLongPackageName() == TestMGMapPath;
				}))
			{
				SelectedSeed = Candidate;
				break;
			}
		}
		if (SelectedSeed == INDEX_NONE)
		{
			AddError(TEXT("No fixture seed from 1 to 64 selected TestMG."));
			return true;
		}
		Dungeon->Seed = SelectedSeed;
		Dungeon->bRandomSeedOnStart = false;
		Saved->bConfigured = true;
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGRuntime(this, Saved));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Saved, PreviousMode, PreviousClients]()
	{
		if (AHBDungeonRuntime* Dungeon = FindEditorDungeon())
		{
			Dungeon->ModuleMaps = Saved->ModuleMaps;
			Dungeon->Seed = Saved->Seed;
			Dungeon->bRandomSeedOnStart = Saved->bRandomSeedOnStart;
		}
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGGameplayMapTest, "HeavenBreach.PCGModule.Encounter.TestMGGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGGameplayMapTest::RunTest(const FString&)
{
	const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay");
	if (!FPackageName::DoesPackageExist(Map) || !FPackageName::DoesPackageExist(TestMGMapPath))
	{ AddError(TEXT("Test_MGGamePlay or TestMG map is missing.")); return false; }
	AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	const TSharedRef<FTestMGRuntimeFixtureState> Fixture = MakeShared<FTestMGRuntimeFixtureState>();
	Fixture->bConfigured = true;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		AHBDungeonRuntime* Dungeon = FindEditorDungeon();
		TestNotNull(TEXT("Test_MGGamePlay has a dungeon runtime"), Dungeon);
		if (Dungeon)
		{
			TestFalse(TEXT("Test_MGGamePlay uses a fixed seed"), Dungeon->bRandomSeedOnStart);
			TestEqual(TEXT("Test_MGGamePlay tested seed"), Dungeon->Seed, 3);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGRuntime(this, Fixture));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients]()
	{
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}

namespace
{
    constexpr int32 DeepAndDeeperSeed = 836511139;
    constexpr int32 In2RoomId = 11;
    constexpr TCHAR In2SourceMap[] = TEXT("/Game/Levels/Floor2/Adventure2-1");

    TSharedRef<FJsonObject> ExportEditableSettings(const UObject& Object)
    {
        const auto Result = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit) || It->HasAnyPropertyFlags(CPF_Transient)) continue;
            FString Value;
            It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(&Object), nullptr,
                const_cast<UObject*>(&Object), PPF_None);
            Result->SetStringField(It->GetName(), Value);
        }
        return Result;
    }

    FString JsonText(const TSharedRef<FJsonObject>& Object)
    {
        FString Text;
        FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
        return Text;
    }

    TMap<FString, bool> SourcePackageDirtyStates()
    {
        TMap<FString, bool> Result;
        for (TObjectIterator<UPackage> It; It; ++It)
            if (It->GetName().StartsWith(TEXT("/Game/")) && !It->HasAnyPackageFlags(PKG_PlayInEditor))
                Result.Add(It->GetName(), It->IsDirty());
        return Result;
    }

    // A test-owned observer, with no production hooks and no strong world/actor references.
    // Capture before Fail clears Rooms; failure strings already own their query evidence.
    struct FDeepAndDeeperObservation : FOutputDevice
    {
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        TArray<FString> Logs;
        TMap<FString, TSharedRef<FJsonObject>> RoomActorCatalog;
        FString LayoutSignature;
        TWeakObjectPtr<AHBDungeonRuntime> Dungeon;
        FDelegateHandle InitializedHandle, TickHandle, LayoutHandle;
        bool bCorrection = false;
        bool bInstalled = false;
        bool bCapturingLogs = false;
        bool bCapturedPoints = false;
        bool bTargetFound = false;
        bool bLoadoutAttempted = false;

        explicit FDeepAndDeeperObservation(bool bInCorrection) : bCorrection(bInCorrection)
        {
            Report->SetStringField(TEXT("Case"), bCorrection ? TEXT("In2CapsuleCorrection") : TEXT("AuthoredBaseline"));
            Report->SetStringField(TEXT("Execution"), TEXT("Standalone PIE; original maps are never saved"));
            Report->SetNumberField(TEXT("RequestedSeed"), DeepAndDeeperSeed);
            Report->SetNumberField(TEXT("ActiveSeed"), 0);
            Report->SetStringField(TEXT("TargetSourceMap"), In2SourceMap);
            Report->SetNumberField(TEXT("TargetRoomId"), In2RoomId);
            Report->SetStringField(TEXT("TargetPointId"), TEXT("In2"));
        }

        virtual ~FDeepAndDeeperObservation() override { Uninstall(); }
        virtual bool IsMemoryOnly() const override { return true; }
        virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName& Category) override
        {
            if (bCapturingLogs && (Category == TEXT("LogHBEncounterSubsystem")
                || Category == TEXT("LogHBChestSpawn") || Category == TEXT("LogHBDungeonRuntime")))
                Logs.Add(Category.ToString() + TEXT(": ") + Message);
        }

        void CaptureLayout()
        {
            if (!Dungeon.IsValid() || Dungeon->Rooms.IsEmpty()) return;
            Report->SetNumberField(TEXT("ActiveSeed"), Dungeon->ActiveSeed);
            Report->SetNumberField(TEXT("GenerationId"), Dungeon->GenerationId);
            TArray<TSharedPtr<FJsonValue>> Layout;
            LayoutSignature.Reset();
            for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
            {
                const auto Item = MakeShared<FJsonObject>();
                Item->SetNumberField(TEXT("RoomId"), Room.Id);
                Item->SetNumberField(TEXT("ParentRoomId"), Room.ParentId);
                Item->SetNumberField(TEXT("Floor"), Room.Floor);
                Item->SetStringField(TEXT("ModuleId"), Room.ModuleId.ToString());
                Item->SetStringField(TEXT("ModuleMap"), Room.ModuleMap.ToSoftObjectPath().ToString());
                Item->SetStringField(TEXT("Transform"), Room.Transform.ToString());
                Item->SetStringField(TEXT("Bounds"), Room.Bounds.ToString());
                Item->SetBoolField(TEXT("IsStartRoom"), Room.bIsStartRoom);
                FString FullRoom;
                FHBPrototypeRoom::StaticStruct()->ExportText(FullRoom, &Room, nullptr, nullptr, PPF_None, nullptr);
                Item->SetStringField(TEXT("FullRoomSnapshot"), FullRoom);
                LayoutSignature += FullRoom + TEXT("\n");
                Layout.Add(MakeShared<FJsonValueObject>(Item));
            }
            Report->SetArrayField(TEXT("Layout"), Layout);
        }

        void CapturePoints()
        {
            if (bCapturedPoints || !Dungeon.IsValid()
                || Dungeon->State != EHBDungeonRuntimeState::PreparingEncounters) return;
            bCapturedPoints = true;
            TArray<TSharedPtr<FJsonValue>> Points;
            for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
            {
                if (Room.Id != In2RoomId) continue;
                const FString SourceMap = Room.ModuleMap.ToSoftObjectPath().GetLongPackageName();
                Report->SetStringField(TEXT("ActualRoom11SourceMap"), SourceMap);
                ULevel* Level = Dungeon->GetLoadedRoomLevel(Room.Id);
                if (!Level) continue;
                TArray<TSharedPtr<FJsonValue>> Classes, Entries;
                for (AActor* Actor : Level->Actors)
                {
                    if (!IsValid(Actor)) continue;
                    const auto ActorInfo = MakeShared<FJsonObject>();
                    ActorInfo->SetStringField(TEXT("ActorPath"), Actor->GetPathName());
                    ActorInfo->SetStringField(TEXT("ActorLabel"), Actor->GetActorLabel());
                    ActorInfo->SetStringField(TEXT("ActorClass"), Actor->GetClass()->GetPathName());
                    ActorInfo->SetStringField(TEXT("Transform"), Actor->GetActorTransform().ToString());
                    TArray<TSharedPtr<FJsonValue>> Components;
                    TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
                    for (const auto* Primitive : Primitives)
                    {
                        const auto Component = MakeShared<FJsonObject>();
                        Component->SetStringField(TEXT("ComponentPath"), Primitive->GetPathName());
                        Component->SetStringField(TEXT("CollisionProfile"), Primitive->GetCollisionProfileName().ToString());
                        Component->SetNumberField(TEXT("CollisionEnabled"), static_cast<int32>(Primitive->GetCollisionEnabled()));
                        Component->SetStringField(TEXT("Bounds"), Primitive->Bounds.GetBox().ToString());
                        if (const auto* Mesh = Cast<UStaticMeshComponent>(Primitive); Mesh && Mesh->GetStaticMesh())
                            Component->SetStringField(TEXT("StaticMesh"), Mesh->GetStaticMesh()->GetPathName());
                        Components.Add(MakeShared<FJsonValueObject>(Component));
                    }
                    ActorInfo->SetArrayField(TEXT("PrimitiveComponents"), Components);
                    RoomActorCatalog.Add(Actor->GetPathName(), ActorInfo);
                    const auto AddEntry = [&Entries](const FString& Kind, const FString& Path, const FVector& Location)
                    {
                        const auto Entry = MakeShared<FJsonObject>();
                        Entry->SetStringField(TEXT("Kind"), Kind);
                        Entry->SetStringField(TEXT("SourcePath"), Path);
                        Entry->SetStringField(TEXT("Location"), Location.ToString());
                        Entries.Add(MakeShared<FJsonValueObject>(Entry));
                    };
                    if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); Portal && IsValid(Portal->TargetPortal))
                        AddEntry(TEXT("ActivePortalArrival"), Portal->GetPathName(), Portal->Arrival->GetComponentLocation());
                    if (const auto* Start = Cast<APlayerStart>(Actor); Start && Room.bIsStartRoom && !Start->IsActorBeingDestroyed())
                        AddEntry(TEXT("StartRoomPlayerStart"), Start->GetPathName(), Start->GetActorLocation());
                    if (auto* Setup = Cast<AHBEncounterSetupActor>(Actor); IsValid(Setup) && Setup->Encounter)
                    {
                        Report->SetObjectField(TEXT("Room11EncounterSettings"), ExportEditableSettings(*Setup->Encounter));
                        for (const FHBEncounterGroup& Group : Setup->Encounter->Groups)
                            for (const FHBEncounterMonsterOption& Option : Group.Monsters)
                            {
                                if (!Option.EnemyClass) continue;
                                const auto Class = MakeShared<FJsonObject>();
                                Class->SetStringField(TEXT("GroupId"), Group.GroupId.ToString());
                                Class->SetStringField(TEXT("ClassPath"), Option.EnemyClass->GetPathName());
                                const auto* Defaults = Option.EnemyClass->GetDefaultObject<AEnemyCharacterBase>();
                                const auto* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
                                if (Capsule)
                                {
                                    Class->SetNumberField(TEXT("CapsuleRadius"), Capsule->GetScaledCapsuleRadius());
                                    Class->SetNumberField(TEXT("CapsuleHalfHeight"), Capsule->GetScaledCapsuleHalfHeight());
                                    Class->SetStringField(TEXT("CollisionProfile"), Capsule->GetCollisionProfileName().ToString());
                                }
                                Classes.Add(MakeShared<FJsonValueObject>(Class));
                            }
                    }
                    auto* Point = Cast<AHBEnemySpawnPoint>(Actor);
                    if (!IsValid(Point) || Point->PointId != TEXT("In2")) continue;
                    const auto Item = MakeShared<FJsonObject>();
                    Item->SetStringField(TEXT("ActorPath"), Point->GetPathName());
                    Item->SetStringField(TEXT("ActorLabel"), Point->GetActorLabel());
                    Item->SetStringField(TEXT("Transform"), Point->GetActorTransform().ToString());
                    Item->SetObjectField(TEXT("AuthoredSettings"), ExportEditableSettings(*Point));
                    // Never identify a room by a transient streaming name or force an old layout.
                    if (SourceMap == In2SourceMap)
                    {
                        bTargetFound = true;
                        if (bCorrection)
                        {
                            Point->bAllowPositionCorrection = true;
                            Point->bUseCapsuleBasedCorrection = true;
                            Point->CorrectionRadiusMultiplier = 2.f;
                        }
                    }
                    Item->SetObjectField(TEXT("EffectiveSettings"), ExportEditableSettings(*Point));
                    Points.Add(MakeShared<FJsonValueObject>(Item));
                }
                // Keep the runtime's entry indexing: portal/start actors, then used connectors.
                for (AActor* Actor : Level->Actors)
                    if (IsValid(Actor))
                    {
                        TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(Actor);
                        for (const auto* Connector : Connectors)
                            if (Room.UsedConnectorIds.Contains(Connector->GetEffectiveConnectorId()))
                            {
                                const auto Entry = MakeShared<FJsonObject>();
                                Entry->SetStringField(TEXT("Kind"), TEXT("UsedConnector"));
                                Entry->SetStringField(TEXT("ConnectorId"), Connector->GetEffectiveConnectorId().ToString());
                                Entry->SetStringField(TEXT("SourcePath"), Connector->GetPathName());
                                Entry->SetStringField(TEXT("Location"), Connector->GetComponentLocation().ToString());
                                Entries.Add(MakeShared<FJsonValueObject>(Entry));
                            }
                    }
                Report->SetArrayField(TEXT("ActiveRoom11Entries"), Entries);
                Report->SetArrayField(TEXT("ConfiguredEnemyClasses"), Classes);
            }
            Report->SetArrayField(TEXT("In2Points"), Points);
            Report->SetBoolField(TEXT("ExactTargetFound"), bTargetFound);
            Report->SetBoolField(TEXT("CorrectionAppliedToPIECopy"), bCorrection && bTargetFound);
        }

        void Install(FAutomationTestBase* Test)
        {
            bInstalled = true;
            if (GLog) GLog->AddOutputDevice(this);
            // AddOutputDevice can replay the backlog. Only subsequent messages belong to this case.
            bCapturingLogs = true;
            InitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda(
                [this, Test](const UWorld::FActorsInitializedParams& Params)
                {
                    UWorld* World = Params.World;
                    if (!World || World->WorldType != EWorldType::PIE || Dungeon.IsValid()) return;
                    for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It)
                    {
                        Dungeon = *It;
                        const bool bBeforeGeneration = Dungeon->State == EHBDungeonRuntimeState::Idle
                            && Dungeon->GenerationId == 0 && !World->HasBegunPlay();
                        Report->SetBoolField(TEXT("SeedAssignedBeforeGeneration"), bBeforeGeneration);
                        Test->TestTrue(TEXT("Fixed seed is assigned before BeginPlay/StartGeneration"), bBeforeGeneration);
                        if (!bBeforeGeneration) return;
                        Dungeon->bRandomSeedOnStart = false;
                        Dungeon->Seed = DeepAndDeeperSeed;
                        Report->SetObjectField(TEXT("PIEDungeonSettings"), ExportEditableSettings(*Dungeon));
                        LayoutHandle = Dungeon->OnLayoutPlanned.AddLambda([this]() { CaptureLayout(); });
                        break;
                    }
                });
            TickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda(
                [this, Test](UWorld* World, ELevelTick, float)
                {
                    if (Dungeon.IsValid() && Dungeon->GetWorld() == World)
                    {
                        // Supply the lobby's default loadout for direct PIE, as in the existing smoke test.
                        auto* PC = World->GetFirstPlayerController();
                        auto* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
                        const auto* Mode = World->GetAuthGameMode();
                        const auto* Defaults = Mode && Mode->DefaultPawnClass
                            ? Cast<AHBPlayerCharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
                        if (!bLoadoutAttempted && PS && !PS->GetSelectedJob() && Defaults && Defaults->GetJobData())
                        {
                            bLoadoutAttempted = true;
                            FHBStartingLoadout Loadout;
                            FText Error;
                            const bool bValid = MakeDefaultStartingLoadout(Defaults->GetJobData(), Loadout, Error)
                                && PS->TrySetStartingLoadout(Loadout, Error);
                            Report->SetBoolField(TEXT("DirectPIEDefaultLoadoutApplied"), bValid);
                            if (!bValid) Test->AddError(Error.ToString());
                        }
                        if (LayoutSignature.IsEmpty()) CaptureLayout();
                        CapturePoints();
                    }
                });
        }

        void Uninstall()
        {
            if (!bInstalled) return;
            bCapturingLogs = false;
            FWorldDelegates::OnWorldInitializedActors.Remove(InitializedHandle);
            FWorldDelegates::OnWorldPreActorTick.Remove(TickHandle);
            if (Dungeon.IsValid()) Dungeon->OnLayoutPlanned.Remove(LayoutHandle);
            if (GLog) GLog->RemoveOutputDevice(this);
            bInstalled = false;
        }

        void Finish(const FString& State, const FString& Status)
        {
            Report->SetStringField(TEXT("RuntimeState"), State);
            Report->SetStringField(TEXT("RuntimeStatus"), Status);
            Report->SetBoolField(TEXT("CapturedBeforeEncounterPreparation"), bCapturedPoints);
            Report->SetBoolField(TEXT("RuntimeCleanupClearedRooms"), Dungeon.IsValid() && Dungeon->Rooms.IsEmpty());
            const FString SpawnLine = FString::Printf(TEXT("Room %d point In2 spawned "), In2RoomId);
            const FString AttemptLine = FString::Printf(TEXT("Room=%d Point=In2 Class="), In2RoomId);
            bool bSpawned = false, bAttempted = false;
            TSet<FString> AttemptedClasses, BlockingActorPaths;
            TArray<TSharedPtr<FJsonValue>> Evidence;
            for (const FString& Line : Logs)
            {
                Evidence.Add(MakeShared<FJsonValueString>(Line));
                bSpawned |= Line.Contains(SpawnLine);
                bAttempted |= Line.Contains(AttemptLine);
                if (Line.Contains(AttemptLine))
                {
                    const int32 ClassStart = Line.Find(TEXT("Class=")) + 6;
                    const int32 ClassEnd = Line.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, ClassStart);
                    AttemptedClasses.Add(Line.Mid(ClassStart, ClassEnd - ClassStart));
                    for (const auto& Pair : RoomActorCatalog)
                        if (Line.Contains(TEXT(" ActorPath=") + Pair.Key + TEXT(" ")))
                            BlockingActorPaths.Add(Pair.Key);
                }
            }
            const bool bRequiredFailure = Status.StartsWith(TEXT("Room 11 encounter:"))
                && Status.Contains(TEXT("required point In2 was not filled."));
            // Required failure owns only In2's class/query evidence, even without Verbose logging.
            if (bRequiredFailure)
            {
                int32 SearchFrom = 0;
                for (int32 ClassTag = Status.Find(TEXT(" Class=")); ClassTag != INDEX_NONE;
                    ClassTag = Status.Find(TEXT(" Class="), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom))
                {
                    const int32 ClassStart = ClassTag + 7;
                    const int32 ClassEnd = Status.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, ClassStart);
                    if (ClassEnd == INDEX_NONE) break;
                    AttemptedClasses.Add(Status.Mid(ClassStart, ClassEnd - ClassStart));
                    SearchFrom = ClassEnd;
                }
                for (const auto& Pair : RoomActorCatalog)
                    if (Status.Contains(TEXT(" ActorPath=") + Pair.Key + TEXT(" "))
                        || Status.Contains(Pair.Key + TEXT(":"))) BlockingActorPaths.Add(Pair.Key);
            }
            Report->SetStringField(TEXT("In2Outcome"), bSpawned ? TEXT("Spawned")
                : bRequiredFailure ? TEXT("RequiredPointUnfilled")
                : bAttempted ? TEXT("Attempted; no spawn observed") : TEXT("Not observed; inspect earlier runtime failure"));
            Report->SetBoolField(TEXT("In2SpawnRolledBack"), bSpawned && State == TEXT("Failed"));
            TArray<TSharedPtr<FJsonValue>> ClassPaths, Blockers;
            TArray<FString> SortedClasses = AttemptedClasses.Array(), SortedBlockers = BlockingActorPaths.Array();
            SortedClasses.Sort();
            SortedBlockers.Sort();
            for (const FString& Path : SortedClasses) ClassPaths.Add(MakeShared<FJsonValueString>(Path));
            for (const FString& Path : SortedBlockers) Blockers.Add(MakeShared<FJsonValueObject>(RoomActorCatalog.FindChecked(Path)));
            Report->SetArrayField(TEXT("In2AttemptedClasses"), ClassPaths);
            Report->SetArrayField(TEXT("ActualIn2BlockingActors"), Blockers);
            Report->SetArrayField(TEXT("RuntimeEvidence"), Evidence);
            Uninstall();
        }
    };

    class FHBObserveDeepAndDeeper : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FDeepAndDeeperObservation> Observation;
        double Started = 0.;
    public:
        FHBObserveDeepAndDeeper(FAutomationTestBase* InTest, TSharedRef<FDeepAndDeeperObservation> InObservation)
            : Test(InTest), Observation(InObservation) {}
        virtual bool Update() override
        {
            if (Started == 0.) Started = FPlatformTime::Seconds();
            auto* Dungeon = Observation->Dungeon.Get();
            if (Dungeon && (Dungeon->State == EHBDungeonRuntimeState::Failed || Dungeon->IsReady()))
            {
                Observation->Finish(Dungeon->IsReady() ? TEXT("Ready") : TEXT("Failed"), Dungeon->Status);
                Test->TestEqual(TEXT("Observed actual generation seed"),
                    static_cast<int32>(Observation->Report->GetNumberField(TEXT("ActiveSeed"))), DeepAndDeeperSeed);
                Test->TestFalse(TEXT("Actual layout was captured before cleanup"), Observation->LayoutSignature.IsEmpty());
                Test->AddInfo(FString::Printf(TEXT("%s: runtime=%s In2=%s"),
                    *Observation->Report->GetStringField(TEXT("Case")),
                    *Observation->Report->GetStringField(TEXT("RuntimeState")),
                    *Observation->Report->GetStringField(TEXT("In2Outcome"))));
                return true;
            }
            const double Timeout = Dungeon ? Dungeon->LoadTimeoutSeconds + Dungeon->EncounterTimeoutSeconds + 30. : 90.;
            if (FPlatformTime::Seconds() - Started < Timeout) return false;
            Observation->Finish(TEXT("ObserverTimeout"), Dungeon ? Dungeon->Status : TEXT("No initialized PIE dungeon"));
            Test->AddError(TEXT("DeepAndDeeper observation did not reach a terminal runtime state."));
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDeepAndDeeperDiagnosticsTest,
    "HeavenBreach.PCGModule.Encounter.DeepAndDeeper.SpawnDiagnostics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDeepAndDeeperDiagnosticsTest::RunTest(const FString&)
{
    const FString Map = TEXT("/Game/Levels/DeepAndDeeper");
    if (!FPackageName::DoesPackageExist(Map)) { AddError(TEXT("DeepAndDeeper map is missing.")); return false; }
    // This is an observation test, not an assertion that the authored dungeon is Ready.
    // Only the two existing fail notifications and the known direct-PIE input asset error are expected.
    AddExpectedErrorPlain(TEXT("LogHBDungeonRuntime: FAILED:"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(TEXT("HeavenBreachGM | Dungeon unavailable; player start blocked |"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    EPlayNetMode PreviousMode;
    int32 PreviousClients;
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
    const auto Baseline = MakeShared<FDeepAndDeeperObservation>(false);
    const auto Correction = MakeShared<FDeepAndDeeperObservation>(true);
    const auto EditorBefore = MakeShared<FString>();
    const auto DirtyBefore = MakeShared<TMap<FString, bool>>();
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Baseline, EditorBefore, DirtyBefore]()
    {
        auto* Dungeon = FindEditorDungeon();
        if (!Dungeon) { AddError(TEXT("DeepAndDeeper has no editor dungeon generator.")); return true; }
        *EditorBefore = JsonText(ExportEditableSettings(*Dungeon));
        *DirtyBefore = SourcePackageDirtyStates();
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
        Baseline->Install(this);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHBObserveDeepAndDeeper(this, Baseline));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Correction]()
    {
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return false;
        Correction->Install(this);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHBObserveDeepAndDeeper(this, Correction));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
        [this, Baseline, Correction, EditorBefore, DirtyBefore, PreviousMode, PreviousClients]()
    {
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return false;
        Baseline->Uninstall();
        Correction->Uninstall();
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
        const auto Report = MakeShared<FJsonObject>();
        Report->SetObjectField(TEXT("Baseline"), Baseline->Report);
        Report->SetObjectField(TEXT("Correction"), Correction->Report);
        const bool bSameLayout = !Baseline->LayoutSignature.IsEmpty()
            && Baseline->LayoutSignature == Correction->LayoutSignature;
        Report->SetBoolField(TEXT("SameActualLayout"), bSameLayout);
        TestTrue(TEXT("Comparison uses the same actual fixed-seed layout"), bSameLayout);
        auto* EditorDungeon = FindEditorDungeon();
        const bool bEditorUnchanged = EditorDungeon && *EditorBefore == JsonText(ExportEditableSettings(*EditorDungeon));
        Report->SetBoolField(TEXT("OriginalEditorDungeonUnchanged"), bEditorUnchanged);
        TestTrue(TEXT("Original editor dungeon settings remain unchanged"), bEditorUnchanged);
        TArray<TSharedPtr<FJsonValue>> DirtyChanges;
        for (const auto& Pair : SourcePackageDirtyStates())
        {
            const bool* Before = DirtyBefore->Find(Pair.Key);
            if ((Before && *Before != Pair.Value) || (!Before && Pair.Value))
                DirtyChanges.Add(MakeShared<FJsonValueString>(Pair.Key));
        }
        Report->SetArrayField(TEXT("SourcePackageDirtyChanges"), DirtyChanges);
        TestTrue(TEXT("No original content package dirty state changed"), DirtyChanges.IsEmpty());
        EPlayNetMode RestoredMode;
        int32 RestoredClients;
        GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(RestoredMode);
        GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(RestoredClients);
        const bool bPlaySettingsRestored = RestoredMode == PreviousMode && RestoredClients == PreviousClients;
        Report->SetBoolField(TEXT("PlaySettingsRestored"), bPlaySettingsRestored);
        TestTrue(TEXT("PIE play settings restored"), bPlaySettingsRestored);
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/PCGDeepAndDeeper");
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Path = Directory / TEXT("SpawnDiagnostics.json");
        TestTrue(TEXT("DeepAndDeeper evidence report saved"), FFileHelper::SaveStringToFile(JsonText(Report), *Path));
        AddInfo(TEXT("Observation test success does not imply dungeon Ready. Evidence: ") + Path);
        return true;
    }));
    return true;
}
#endif
