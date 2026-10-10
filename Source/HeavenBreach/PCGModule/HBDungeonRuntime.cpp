#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Escape/ExtractionZone.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "PCGComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BrushComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "CollisionQueryParams.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PhysicsEngine/BodySetup.h"
DEFINE_LOG_CATEGORY_STATIC(LogHBDungeonRuntime, Log, All);

AHBDungeonRuntime::AHBDungeonRuntime()
{
    PrimaryActorTick.bCanEverTick = true;
    auto* Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GenerationBounds"));
    RootComponent = Bounds;
    Bounds->InitBoxExtent(FVector(180000, 80000, 15000));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetHiddenInGame(true);
    PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
    PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
#if WITH_EDITORONLY_DATA
    PCG->bRegenerateInEditor = false;
#endif
    DoorSealClass = AHBDungeonDoorSeal::StaticClass();
    Status = TEXT("Waiting for HeavenBreachGameMode.");
}
void AHBDungeonRuntime::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_Client)
    {
        if (State == EHBDungeonRuntimeState::Idle) Status = TEXT("Waiting for server layout.");
        BindSnapshotSource();
        return;
    }
    if (!Cast<AHeavenBreachGameMode>(GetWorld()->GetAuthGameMode()))
        Fail(TEXT("Gameplay dungeon requires HeavenBreachGameMode (or its Blueprint subclass)."));
}
void AHBDungeonRuntime::RefreshModuleCatalog()
{
    if (GetWorld() && GetWorld()->IsGameWorld()) return;
    const bool bValid = HBDungeonModuleCatalog::Build(ModuleMaps, ModuleCatalog, CatalogErrors);
    Status = bValid ? TEXT("Catalog validated. Configure transitions and non-boss start candidates with PlayerStarts.") : FString::Join(CatalogErrors, TEXT("\n"));
}
void AHBDungeonRuntime::StartGeneration()
{
    if (State != EHBDungeonRuntimeState::Idle) return;
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    if (!CanPlanGeneration()) return;
    if (const auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>())
        GenerationId = FMath::Max(GenerationId, Source->GetDungeonSnapshot().GenerationId);
    ++GenerationId;
    int32 Count = 0;
    for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It) ++Count;
    if (Count != 1) { Fail(TEXT("Place exactly one gameplay dungeon generator.")); return; }
    for (TActorIterator<AHBPCGPreviewGenerator> It(GetWorld()); It; ++It)
    { Fail(TEXT("Remove the Preview generator from the gameplay map.")); return; }
    if (!Graph) { Fail(TEXT("Select the dungeon Plan -> Load PCG graph.")); return; }
    FString SettingsError;
    if (!ValidateLocalSettings(SettingsError)) { Fail(SettingsError); return; }
    if (!FMath::IsFinite(EncounterTimeoutSeconds) || EncounterTimeoutSeconds <= 0)
    { Fail(TEXT("Encounter timeout must be positive and finite.")); return; }
    if (!HBDungeonModuleCatalog::Build(ModuleMaps, ModuleCatalog, CatalogErrors))
    { Fail(FString::Join(CatalogErrors, TEXT("\n"))); return; }
    ActiveSeed = bRandomSeedOnStart ? static_cast<int32>(FGuid::NewGuid().A & 0x7fffffff) : Seed;
    ActiveRules = GetGenerationRules();
    ActiveDoorSealClass = DoorSealClass;
    ActiveDoorSealExtent = DoorSealExtent;
    ActiveSettings.Seed = ActiveSeed;
    ActiveSettings.Origin = GetActorTransform();
    ActiveSettings.bAllowModuleReuse = bAllowModuleReuse;
    ActiveSettings.MaxSearchSteps = MaxConnectionAttempts;
    ActiveSettings.MaxSearchSeconds = ConnectionSearchSeconds;
    GenerationStartTime = FPlatformTime::Seconds();
    State = EHBDungeonRuntimeState::Generating;
    Status = TEXT("Planning dungeon...");
    PCG->Seed = ActiveSeed;
    PCG->SetGraphLocal(Graph);
    PCG->GenerateLocal(true);
}
FHBDungeonGenerationRules AHBDungeonRuntime::GetGenerationRules() const
{
    auto Rules = DungeonRules;
    Rules.PortalPlacementMode = PortalPlacementMode;
    Rules.PortalPairCount = PortalPairCount;
    Rules.MaxConsecutiveAdventureRooms = MaxConsecutiveAdventureRooms;
    Rules.MaxConsecutiveRecoveryRooms = MaxConsecutiveRecoveryRooms;
    Rules.MaxConsecutiveEnhancementRooms = MaxConsecutiveEnhancementRooms;
    Rules.MaxRecoveryRoomsPerFloor = MaxRecoveryRoomsPerFloor;
    Rules.MaxEnhancementRoomsPerFloor = MaxEnhancementRoomsPerFloor;
    return Rules;
}
bool AHBDungeonRuntime::PlanLayout()
{
    if (!CanPlanGeneration() || State != EHBDungeonRuntimeState::Generating || !Rooms.IsEmpty()) return false;
    FString Error;
    if (!HBDungeonRuleLayout::Generate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error))
    { Fail(Error); return false; }
    if (!HBDungeonRuleLayout::Validate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error)
        || !HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error))
    { Fail(Error); return false; }
    auto* GameState = GetWorld()->GetGameState<AHeavenBreachGameState>();
    if (!GameState || !GameState->PublishDungeonLayout(GenerationId, ActiveSeed, Rooms, ActiveDoorSealClass, ActiveDoorSealExtent))
    { Fail(TEXT("Could not publish dungeon layout to HeavenBreachGameState.")); return false; }
    OnLayoutPlanned.Broadcast();
    return true;
}
void AHBDungeonRuntime::MaterializeLayout(int32 PointCount)
{
    if (!CanPlanGeneration() || State != EHBDungeonRuntimeState::Generating || !Streams.IsEmpty()) return;
    FString Error;
    if (PointCount != Rooms.Num() || Rooms.IsEmpty())
    { Fail(TEXT("PCG point count does not match the planned layout.")); return; }
    if (!HBDungeonRuleLayout::Validate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error))
    { Fail(Error); return; }
    LoadLocalRooms();
}
bool AHBDungeonRuntime::CanPlanGeneration() const
{
    // A non-replicated map actor can have authority even in a client world.
    return !bEndingPlay && GetWorld() && GetWorld()->IsGameWorld() && HasAuthority()
        && (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer);
}
bool AHBDungeonRuntime::ResetGeneration()
{
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
    if (!CanPlanGeneration() || (Mode && Mode->GetMatchState() != MatchState::WaitingToStart)) return false;
    State = EHBDungeonRuntimeState::Idle;
    if (auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>()) Source->InvalidateDungeonLayout(GenerationId);
    ClearGenerated();
    Status = TEXT("Waiting for a new server dungeon generation.");
    return true;
}
bool AHBDungeonRuntime::ValidateLocalSettings(FString& Error) const
{
    if (GetNetMode() != NM_Client && !AHBDungeonDoorSeal::ValidateSettings(DoorSealClass, DoorSealExtent, Error)) return false;
    if (!FMath::IsFinite(LoadTimeoutSeconds) || LoadTimeoutSeconds <= 0)
    { Error = TEXT("Load timeout must be positive and finite."); return false; }
    return true;
}
bool AHBDungeonRuntime::ApplyServerLayout(const FHBDungeonSnapshot& Snapshot)
{
    if (bEndingPlay || !GetWorld() || !GetWorld()->IsGameWorld() || GetNetMode() != NM_Client
        || Snapshot.State != EHBDungeonSnapshotState::Active || Snapshot.GenerationId <= 0) return false;
    if (Snapshot.GenerationId < GenerationId) return false;
    if (Snapshot.GenerationId == GenerationId) return State == EHBDungeonRuntimeState::Loading || IsReady();
    FString Error;
    const TSubclassOf<AHBDungeonDoorSeal> SealClass = Snapshot.DoorSealClass.LoadSynchronous();
    if (!ValidateLocalSettings(Error) || !AHBDungeonDoorSeal::ValidateSettings(SealClass, Snapshot.DoorSealExtent, Error)
        || !HBDungeonRuntimeValidation::ValidateLayoutForLoading(Snapshot.Rooms, Error))
    {
        UE_LOG(LogHBDungeonRuntime, Warning, TEXT("Rejected server layout: %s"), *Error);
        return false;
    }
    ClearGenerated();
    GenerationId = Snapshot.GenerationId;
    ActiveSeed = Snapshot.Seed;
    Rooms = Snapshot.Rooms;
    ActiveDoorSealClass = SealClass;
    ActiveDoorSealExtent = Snapshot.DoorSealExtent;
    ModuleCatalog.Reset();
    GenerationStartTime = FPlatformTime::Seconds();
    LoadLocalRooms();
    return State == EHBDungeonRuntimeState::Loading;
}
void AHBDungeonRuntime::LoadLocalRooms()
{
    FString Error;
    if (!HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error)) { Fail(Error); return; }
    State = EHBDungeonRuntimeState::Loading;
    for (const auto& Room : Rooms)
    {
        bool bSuccess = false;
        auto* Stream = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, Room.ModuleMap, Room.Transform,
            bSuccess, FString::Printf(TEXT("HBDungeon_G%d_R%d"), GenerationId, Room.Id));
        if (!bSuccess || !Stream) { Fail(FString::Printf(TEXT("Failed to load room %d."), Room.Id)); return; }
        Streams.Add(Stream);
    }
    Status = TEXT("Loading local dungeon rooms...");
}
bool AHBDungeonRuntime::SpawnDoorSeals(const FHBPrototypeRoom& Room,
    const TArray<FHBDungeonConnectorDefinition>& Connectors, FString& Error)
{
    if (!AHBDungeonDoorSeal::ValidateSettings(ActiveDoorSealClass, ActiveDoorSealExtent, Error)) return false;
    ULevel* Level = GetLoadedRoomLevel(Room.Id);
    if (!Level) { Error = TEXT("Missing streamed room for door seals."); return false; }
    for (int32 Index = 0; Index < Room.UnusedConnectorIds.Num(); ++Index)
    {
        const FName Id = Room.UnusedConnectorIds[Index];
        const auto* Connector = Connectors.FindByPredicate([Id](const auto& C) { return C.ConnectorId == Id; });
        if (!Connector) { Error = TEXT("Layout references a missing unused connector."); return false; }
        FTransform Transform = Connector->LocalTransform * Room.Transform;
        Transform.AddToTranslation(Transform.GetRotation().RotateVector(FVector(0, 0, ActiveDoorSealExtent.Z)));
        FActorSpawnParameters Params; Params.Owner = this;
        Params.OverrideLevel = Level;
        Params.Name = FName(*FString::Printf(TEXT("HBDungeonSeal_G%d_R%d_C%d"), GenerationId, Room.Id, Index));
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Seal = GetWorld()->SpawnActor<AHBDungeonDoorSeal>(ActiveDoorSealClass, Transform, Params);
        if (!Seal) { Error = TEXT("Could not create unused-door seal."); return false; }
        if (!Seal->SetExtent(ActiveDoorSealExtent))
        {
            Seal->Destroy();
            Error = TEXT("Spawned door seal changed its mesh, replication or collision settings.");
            return false;
        }
        Seals.Add(Seal);
    }
    return true;
}
bool AHBDungeonRuntime::PrepareClientGameplay(FString& Error)
{
    if (!HBDungeonRuntimeValidation::ValidateLocalPortalInstances(Rooms, Streams, Error)) return false;
    for (const auto& Room : Rooms)
    {
        AHBDungeonModuleDefinition* Definition = nullptr;
        for (AActor* Actor : Streams[Room.Id]->GetLoadedLevel()->Actors)
            if (auto* Candidate = Cast<AHBDungeonModuleDefinition>(Actor))
            {
                if (Definition) { Error = TEXT("Multiple module definitions in a local room."); return false; }
                Definition = Candidate;
            }
        TArray<FHBDungeonConnectorDefinition> Connectors;
        TArray<FHBDungeonOccupancyDefinition> Occupancy;
        TArray<FHBDungeonModuleIssue> Issues;
        if (!Definition || Definition->ModuleId != Room.ModuleId
            || !Definition->ReadModuleGeometry(Room.Transform, Connectors, Occupancy, Issues))
        { Error = TEXT("Loaded client room does not match the authoritative module."); return false; }
        if (!SpawnDoorSeals(Room, Connectors, Error)) return false;
    }
    TArray<APlayerStart*> Starts;
    return PrepareRoomPlayerStarts(Starts, Error);
}
bool AHBDungeonRuntime::PrepareRoomPlayerStarts(TArray<APlayerStart*>& OutStarts, FString& Error)
{
    TArray<ULevel*> Levels;
    for (const auto& Room : Rooms) Levels.Add(GetLoadedRoomLevel(Room.Id));
    return HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, OutStarts, Error);
}
bool AHBDungeonRuntime::PrepareServerGameplay(FString& Error)
{
    if (!HBDungeonRuleLayout::Validate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error)) return false;
    for (const auto& Room : Rooms)
    {
        ULevel* Level = Streams[Room.Id]->GetLoadedLevel();
        const auto* Module = ModuleCatalog.FindByPredicate([&](const auto& M) { return M.ModuleId == Room.ModuleId; });
        AHBDungeonModuleDefinition* Definition = nullptr;
        for (AActor* Actor : Level->Actors)
            if (auto* D = Cast<AHBDungeonModuleDefinition>(Actor)) { Definition = D; break; }
        TArray<FHBDungeonConnectorDefinition> Connectors;
        TArray<FHBDungeonOccupancyDefinition> Occupancy;
        TArray<FHBDungeonModuleIssue> Issues;
        if (!Module || !Definition || Definition->ModuleId != Room.ModuleId
            || !Definition->RoomInfo || Definition->RoomInfo->bIsStartCandidate != Module->bIsStartCandidate
            || !Definition->ReadModuleGeometry(Room.Transform, Connectors, Occupancy, Issues)
            || Connectors.Num() != Module->Connectors.Num() || Occupancy.Num() != Module->Occupancy.Num())
        { Error = FString::Printf(TEXT("Room %d no longer matches the source module."), Room.Id); return false; }
        for (int32 I = 0; I < Connectors.Num(); ++I)
            if (Connectors[I].ConnectorId != Module->Connectors[I].ConnectorId
				|| Connectors[I].Role != Module->Connectors[I].Role
                || !Connectors[I].LocalTransform.Equals(Module->Connectors[I].LocalTransform, 0.1))
            { Error = TEXT("Loaded connector geometry differs from the plan."); return false; }
        for (int32 I = 0; I < Occupancy.Num(); ++I)
            if (!Occupancy[I].LocalTransform.Equals(Module->Occupancy[I].LocalTransform, 0.1)
                || !Occupancy[I].Extent.Equals(Module->Occupancy[I].Extent, 0.1))
            { Error = TEXT("Loaded occupancy geometry differs from the plan."); return false; }
        if (!SpawnDoorSeals(Room, Connectors, Error)) return false;
    }
    if (!HBDungeonRuntimeValidation::LinkPortals(Rooms, ModuleCatalog, Streams, LinkedPortals, Error)) return false;
    TArray<APlayerStart*> Starts;
    if (!PrepareRoomPlayerStarts(Starts, Error)) return false;
    const auto* Mode = GetWorld()->GetAuthGameMode();
    const auto* Character = Mode && Mode->DefaultPawnClass ? Cast<ACharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
    if (!Character) { Error = TEXT("Gameplay dungeon requires a Character default pawn."); return false; }
    const auto* Capsule = Character->GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    StartPoint = HBDungeonRuntimeValidation::SelectPlayerStart(Starts, ActiveSeed, [&](const APlayerStart& Start)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HBDungeonSpawn), false);
        for (const APlayerStart* Point : Starts) Query.AddIgnoredActor(Point);
        if (GetWorld()->OverlapBlockingTestByProfile(Start.GetActorLocation(), Start.GetActorQuat(), Capsule->GetCollisionProfileName(), Shape, Query)) return false;
        FHitResult Ground;
        return GetWorld()->LineTraceSingleByChannel(Ground, Start.GetActorLocation(),
            Start.GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 100), ECC_WorldStatic, Query);
    }, Error);
    return StartPoint != nullptr;
}
APlayerStart* AHBDungeonRuntime::GetPlayerStart() const { return IsReady() ? StartPoint.Get() : nullptr; }
ULevel* AHBDungeonRuntime::GetLoadedRoomLevel(int32 RoomId) const
{
	return Streams.IsValidIndex(RoomId) && IsValid(Streams[RoomId]) ? Streams[RoomId]->GetLoadedLevel() : nullptr;
}
void AHBDungeonRuntime::BindSnapshotSource()
{
    auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>();
    if (SnapshotSource.Get() == Source) return;
    if (SnapshotSource.IsValid()) SnapshotSource->OnDungeonSnapshotChanged.Remove(SnapshotChangedHandle);
    SnapshotSource = Source;
    SnapshotChangedHandle.Reset();
    if (!Source) return;
    SnapshotChangedHandle = Source->OnDungeonSnapshotChanged.AddUObject(this, &AHBDungeonRuntime::ConsumeServerSnapshot);
    // The replicated snapshot may already exist when the local map actor subscribes.
    ConsumeServerSnapshot();
}
void AHBDungeonRuntime::ConsumeServerSnapshot()
{
    if (bEndingPlay || GetNetMode() != NM_Client || !SnapshotSource.IsValid()) return;
    const auto& Snapshot = SnapshotSource->GetDungeonSnapshot();
    if (Snapshot.GenerationId <= 0 || Snapshot.GenerationId < GenerationId) return;
    if (Snapshot.State == EHBDungeonSnapshotState::Cleared || Snapshot.State == EHBDungeonSnapshotState::Failed)
    {
        const auto TerminalState = Snapshot.State == EHBDungeonSnapshotState::Failed
            ? EHBDungeonRuntimeState::Failed : EHBDungeonRuntimeState::Idle;
        if (Snapshot.GenerationId == GenerationId && State == TerminalState)
        {
            Status = Snapshot.State == EHBDungeonSnapshotState::Failed ? Snapshot.FailureReason : TEXT("Server cleared this dungeon generation.");
            return;
        }
        State = TerminalState;
        ClearGenerated();
        GenerationId = Snapshot.GenerationId;
        Status = Snapshot.State == EHBDungeonSnapshotState::Failed ? Snapshot.FailureReason : TEXT("Server cleared this dungeon generation.");
        // This terminal snapshot is already known by the server. Do not echo a failure RPC.
        if (State == EHBDungeonRuntimeState::Failed) OnFailed.Broadcast(Status);
        return;
    }
    if (Snapshot.State != EHBDungeonSnapshotState::Active) return;
    if (Snapshot.GenerationId <= GenerationId) return;
    if (!ApplyServerLayout(Snapshot))
        Fail(TEXT("Could not apply replicated dungeon layout."));
}
bool AHBDungeonRuntime::CreateDungeonNavigationBounds(FString& Error)
{
    if (!CanPlanGeneration()) { Error = TEXT("Dungeon navigation requires the server world."); return false; }
    if (IsValid(DungeonNavigationBounds)) return true;
    UWorld* World = GetWorld();
    UNavigationSystemV1* Nav = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    if (!World || !Nav || Rooms.IsEmpty())
    { Error = TEXT("Dungeon navigation system or planned rooms are unavailable."); return false; }

    FBox DungeonBounds(ForceInit);
    for (const FHBPrototypeRoom& Room : Rooms)
    {
        if (!Room.Bounds.IsValid)
        { Error = FString::Printf(TEXT("Room %d has invalid navigation bounds."), Room.Id); return false; }
        DungeonBounds += Room.Bounds;
    }
    DungeonBounds = DungeonBounds.ExpandBy(FVector(500, 500, 500));
    const FVector Extent = DungeonBounds.GetExtent();
    if (Extent.ContainsNaN() || Extent.GetMin() <= 0.f)
    { Error = TEXT("Dungeon navigation bounds have invalid size."); return false; }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.OverrideLevel = World->PersistentLevel;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(
        ANavMeshBoundsVolume::StaticClass(), DungeonBounds.GetCenter(), FRotator::ZeroRotator, Params);
    UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
    if (!Brush)
    {
        if (IsValid(Volume)) Volume->Destroy();
        Error = TEXT("Could not create the dungeon navigation bounds volume.");
        return false;
    }
    UBodySetup* Body = NewObject<UBodySetup>(Brush);
    FKBoxElem Box;
    Box.X = Extent.X * 2.f;
    Box.Y = Extent.Y * 2.f;
    Box.Z = Extent.Z * 2.f;
    Body->AggGeom.BoxElems.Add(Box);
    Brush->BrushBodySetup = Body;
    Brush->UpdateBounds();
    const FBox ActualBounds = Volume->GetComponentsBoundingBox(true);
    // The box extent is stored as floats; center +/- extent can differ by a fraction of a cm.
    const FBox BoundsWithRoundingTolerance = ActualBounds.ExpandBy(1.f);
    if (!ActualBounds.IsValid || !BoundsWithRoundingTolerance.IsInsideOrOn(DungeonBounds.Min)
        || !BoundsWithRoundingTolerance.IsInsideOrOn(DungeonBounds.Max))
    {
        Volume->Destroy();
        Error = FString::Printf(TEXT("Runtime navigation volume does not cover the planned dungeon. Planned=%s Actual=%s"),
            *DungeonBounds.ToString(), *ActualBounds.ToString());
        return false;
    }
    DungeonNavigationBounds = Volume;
    Nav->OnNavigationBoundsUpdated(Volume);
    return true;
}
bool AHBDungeonRuntime::IsEncounterNavigationReady() const
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* Nav = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    if (!Nav || !IsValid(DungeonNavigationBounds)
        || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)
        || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) return false;
    for (const FHBPrototypeRoom& Room : Rooms)
    {
        ULevel* Level = GetLoadedRoomLevel(Room.Id);
        if (!Level) continue;
        const bool bHasEncounter = UHBEncounterSubsystem::HasEncounter(Level);
        const bool bHasChests = UHBChestSpawnSubsystem::HasPlacement(Level);
        if (!bHasEncounter && !bHasChests) continue;
        bool bProjectedEnemy = !bHasEncounter;
        bool bProjectedChest = !bHasChests;
        for (AActor* Actor : Level->Actors)
        {
            if (!IsValid(Actor) || Actor->GetLevel() != Level) continue;
            FNavLocation OnNav;
            if (!bProjectedEnemy)
                if (const AHBEnemySpawnPoint* Point = Cast<AHBEnemySpawnPoint>(Actor))
                    bProjectedEnemy = Nav->ProjectPointToNavigation(
                        Point->GetActorLocation(), OnNav, FVector(150, 150, 350));
            if (!bProjectedChest)
                if (const AHBChestSpawnPoint* Point = Cast<AHBChestSpawnPoint>(Actor))
                    bProjectedChest = Nav->ProjectPointToNavigation(
                        Point->GetActorLocation() + Point->GetActorForwardVector() * 120.f,
                        OnNav, FVector(100, 100, 200));
            if (bProjectedEnemy && bProjectedChest) break;
        }
        if (!bProjectedEnemy || !bProjectedChest) return false;
    }
    return true;
}
bool AHBDungeonRuntime::PrepareEncounters(FString& Error)
{
    if (!CanPlanGeneration()) { Error = TEXT("Room spawn preparation requires the server world."); return false; }
	UHBEncounterSubsystem* EncounterSubsystem = GetWorld()->GetSubsystem<UHBEncounterSubsystem>();
	if (!EncounterSubsystem) { Error = TEXT("Encounter subsystem is unavailable."); return false; }
    UHBChestSpawnSubsystem* ChestSubsystem = GetWorld()->GetSubsystem<UHBChestSpawnSubsystem>();
    if (!ChestSubsystem) { Error = TEXT("Chest spawn subsystem is unavailable."); return false; }
    for (const auto& Room : Rooms)
    {
        if (!Streams.IsValidIndex(Room.Id) || !IsValid(Streams[Room.Id]))
        { Error = FString::Printf(TEXT("Missing streamed room %d for encounter preparation."), Room.Id); return false; }
        ULevel* Level = Streams[Room.Id]->GetLoadedLevel();
        if (!Level || !Streams[Room.Id]->IsLevelVisible())
        { Error = FString::Printf(TEXT("Room %d is not loaded and visible for spawn preparation."), Room.Id); return false; }
        AHBDungeonModuleDefinition* Definition = nullptr;
        for (AActor* Actor : Level->Actors)
            if (auto* Candidate = Cast<AHBDungeonModuleDefinition>(Actor)) { Definition = Candidate; break; }
        if (!Definition) { Error = FString::Printf(TEXT("Room %d has no module definition."), Room.Id); return false; }
        const bool bHasEncounter = UHBEncounterSubsystem::HasEncounter(Level);
        const bool bHasChests = UHBChestSpawnSubsystem::HasPlacement(Level);
        if (!bHasEncounter && !bHasChests) continue;
        TArray<FHBRoomSpawnEntry> Entries;
        for (AActor* Actor : Level->Actors)
        {
            if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal) && IsValid(Portal->TargetPortal))
                Entries.Emplace(Portal->Arrival->GetComponentLocation());
            if (const auto* Start = Cast<APlayerStart>(Actor); Room.bIsStartRoom && IsValid(Start) && !Start->IsActorBeingDestroyed())
                Entries.Emplace(Start->GetActorLocation());
        }
        for (AActor* Actor : Level->Actors)
            if (IsValid(Actor))
            {
                TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(Actor);
                for (const auto* Connector : Connectors)
                    if (Room.UsedConnectorIds.Contains(Connector->GetEffectiveConnectorId()))
                        Entries.Emplace(Connector->GetComponentLocation(), Connector->GetNavigationEntryLocation());
            }
        if (bHasEncounter && (!EncounterSubsystem->StartEncounterWithEntries(
            GenerationId, ActiveSeed, Room.Id, Level, Room.Bounds, Entries, Error, true)
            || !EncounterSubsystem->IsPreparedFor(GenerationId, Room.Id, Level)))
        {
            if (Error.IsEmpty()) Error = TEXT("Encounter did not register a prepared room.");
            Error = FString::Printf(TEXT("Room %d encounter: %s"), Room.Id, *Error);
            return false;
        }
        if (bHasChests && (!ChestSubsystem->StartPlacementWithEntries(
            GenerationId, ActiveSeed, Room.Id, Level, Room.Bounds, Entries, Error)
            || !ChestSubsystem->IsPreparedFor(GenerationId, Room.Id, Level)))
        {
            if (Error.IsEmpty()) Error = TEXT("Chest placement did not register a prepared room.");
            Error = FString::Printf(TEXT("Room %d chest placement: %s"), Room.Id, *Error);
            return false;
        }
    }
    return true;
}
bool AHBDungeonRuntime::ActivateEncounters(FString& Error)
{
    Error.Reset();
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
    if (!CanPlanGeneration() || !IsReady() || !Mode || !Mode->IsMatchInProgress() || !Mode->AreDungeonPlayersLoaded())
    { Error = TEXT("Dungeon encounter activation requires a ready server dungeon and a started match with every player loaded."); return false; }
    if (bEncountersActive) return true;
    auto* Encounters = GetWorld()->GetSubsystem<UHBEncounterSubsystem>();
    if (!Encounters) { Error = TEXT("Encounter subsystem is unavailable for activation."); return false; }
    bool bHasEncounters = false;
    for (const auto& Room : Rooms)
    {
        ULevel* Level = GetLoadedRoomLevel(Room.Id);
        if (!IsValid(Level)) { Error = TEXT("Encounter activation requires every dungeon room to remain loaded."); return false; }
        if (!UHBEncounterSubsystem::HasEncounter(Level)) continue;
        bHasEncounters = true;
        if (!Encounters->IsPreparedFor(GenerationId, Room.Id, Level))
        { Error = TEXT("An encounter room is not prepared for the current dungeon generation."); return false; }
    }
    if (bHasEncounters && !Encounters->ActivateGeneration(GenerationId, Error)) return false;
    bEncountersActive = true;
    return true;
}

void AHBDungeonRuntime::SelectExtractionPoints()
{
    struct FCandidate
    {
        AExtractionZone* Zone = nullptr;
        int32 RoomId = INDEX_NONE;
        int32 Floor = 0;
        bool bPreferred = true;
        double Weight = 1.0;
        FVector Location = FVector::ZeroVector;
        FString Key;
    };
    ActiveExtractionPoints.Reset();
    auto RoomById = [this](int32 Id) -> const FHBPrototypeRoom*
    {
        return Rooms.FindByPredicate([Id](const FHBPrototypeRoom& R) { return R.Id == Id; });
    };
    auto FloorDepth = [&](const FHBPrototypeRoom& Room)
    {
        int32 Result = 0;
        const FHBPrototypeRoom* Current = &Room;
        for (int32 Guard = 0; Guard < Rooms.Num(); ++Guard)
        {
            const FHBPrototypeRoom* Parent = Current->ParentId == INDEX_NONE ? nullptr : RoomById(Current->ParentId);
            if (!Parent || Parent->Floor != Room.Floor) break;
            ++Result;
            Current = Parent;
        }
        return Result;
    };
    TArray<FCandidate> Candidates;
    TSet<int32> FloorSet;
    for (const FHBPrototypeRoom& Room : Rooms)
    {
        FloorSet.Add(Room.Floor);
        ULevel* Level = GetLoadedRoomLevel(Room.Id);
        if (!Level) continue;
        const int32 Depth = FloorDepth(Room);
        const bool bAllowedRoom = Room.Role == EHBPrototypeRoomRole::Adventure && !Room.bCorridor && !Room.bIsStartRoom;
        for (AActor* Actor : Level->Actors)
        {
            AExtractionZone* Zone = Cast<AExtractionZone>(Actor);
            if (!IsValid(Zone) || !Zone->IsExtractionCandidate()) continue;
            if (!bAllowedRoom) { Zone->SetExtractionActive(false); continue; }
            FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
            Candidate.Zone = Zone;
            Candidate.RoomId = Room.Id;
            Candidate.Floor = Room.Floor;
            Candidate.bPreferred = Depth >= MinExtractionRoomDepth;
            Candidate.Weight = FMath::Max(0.01, static_cast<double>(Zone->GetSelectionWeight())) * (1.0 + ExtractionDepthWeight * Depth);
            Candidate.Location = Zone->GetActorLocation();
            Candidate.Key = FString::Printf(TEXT("%05d:%s"), Room.Id, *Zone->GetFName().ToString());
        }
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Key < B.Key; });
    TArray<int32> Floors = FloorSet.Array();
    Floors.Sort();
    const int32 MinCount = FMath::Max(1, MinExtractionPointsPerFloor);
    const int32 MaxCount = FMath::Max(MinCount, MaxExtractionPointsPerFloor);
    TSet<AExtractionZone*> Selected;
    for (const int32 Floor : Floors)
    {
        const uint32 StreamSeed = static_cast<uint32>(ActiveSeed) * 2654435761u ^ static_cast<uint32>(Floor * 7919 + 101);
        FRandomStream Stream(static_cast<int32>(StreamSeed & 0x7fffffff));
        const int32 Wanted = Stream.RandRange(MinCount, MaxCount);
        int32 FloorCandidates = 0;
        for (const FCandidate& Candidate : Candidates)
            if (Candidate.Floor == Floor) ++FloorCandidates;
        TArray<int32> Picked;
        auto PickUntil = [&](int32 Target, bool bPreferredOnly, bool bSeparate, bool bDistinctRoom)
        {
            while (Picked.Num() < Target)
            {
                TArray<int32> Valid;
                double Total = 0;
                for (int32 Index = 0; Index < Candidates.Num(); ++Index)
                {
                    const FCandidate& Candidate = Candidates[Index];
                    if (Candidate.Floor != Floor || Picked.Contains(Index) || (bPreferredOnly && !Candidate.bPreferred)) continue;
                    bool bAllowed = true;
                    for (const int32 Other : Picked)
                        if ((bDistinctRoom && Candidates[Other].RoomId == Candidate.RoomId)
                            || (bSeparate && FVector::Dist(Candidates[Other].Location, Candidate.Location) < MinExtractionSeparation))
                        { bAllowed = false; break; }
                    if (!bAllowed) continue;
                    Valid.Add(Index);
                    Total += Candidate.Weight;
                }
                if (Valid.IsEmpty() || Total <= 0) return;
                double Roll = Stream.FRand() * Total;
                int32 Choice = Valid.Last();
                for (const int32 Index : Valid)
                {
                    Roll -= Candidates[Index].Weight;
                    if (Roll <= 0) { Choice = Index; break; }
                }
                Picked.Add(Choice);
            }
        };
        PickUntil(Wanted, true, true, true);
        PickUntil(MinCount, true, false, true);
        PickUntil(MinCount, false, false, true);
        PickUntil(MinCount, false, false, false);
        if (Picked.Num() < Wanted)
            UE_LOG(LogHBDungeonRuntime, Warning, TEXT("Extraction Floor=%d wanted %d, selected %d of %d candidates."),
                Floor, Wanted, Picked.Num(), FloorCandidates);
        if (Picked.IsEmpty())
            UE_LOG(LogHBDungeonRuntime, Error, TEXT("Extraction Floor=%d has no extraction point. Add candidates to more Adventure modules."), Floor);
        for (const int32 Index : Picked)
        {
            Selected.Add(Candidates[Index].Zone);
            UE_LOG(LogHBDungeonRuntime, Display, TEXT("Extraction Floor=%d Room=%d Point=%s Weight=%.2f Preferred=%d"),
                Floor, Candidates[Index].RoomId, *Candidates[Index].Zone->GetName(), Candidates[Index].Weight,
                Candidates[Index].bPreferred ? 1 : 0);
        }
    }
    for (const FCandidate& Candidate : Candidates)
    {
        const bool bActive = Selected.Contains(Candidate.Zone);
        Candidate.Zone->SetExtractionActive(bActive);
        if (bActive) ActiveExtractionPoints.Add(Candidate.Zone);
    }
    UE_LOG(LogHBDungeonRuntime, Display, TEXT("Extraction points active=%d candidates=%d seed=%d"),
        ActiveExtractionPoints.Num(), Candidates.Num(), ActiveSeed);
}
void AHBDungeonRuntime::MarkReady()
{
    State = EHBDungeonRuntimeState::Ready;
    Status = GetNetMode() == NM_Client ? TEXT("READY - local dungeon rooms prepared.")
        : TEXT("READY - server dungeon and room spawns prepared.");
    UE_LOG(LogHBDungeonRuntime, Display, TEXT("%s Seed=%d Rooms=%d Seals=%d"), *Status, ActiveSeed, Rooms.Num(), Seals.Num());
    OnReady.Broadcast();
}
void AHBDungeonRuntime::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bEndingPlay) return;
    if (GetNetMode() == NM_Client) BindSnapshotSource();
    if (State != EHBDungeonRuntimeState::Generating && State != EHBDungeonRuntimeState::Loading
        && State != EHBDungeonRuntimeState::PreparingEncounters) return;
    if (State != EHBDungeonRuntimeState::PreparingEncounters && FPlatformTime::Seconds() - GenerationStartTime > LoadTimeoutSeconds)
    { Fail(TEXT("Dungeon generation/loading timed out. Check graph and source maps.")); return; }
    if (State == EHBDungeonRuntimeState::PreparingEncounters)
    {
        if (!CanPlanGeneration()) { Fail(TEXT("Only the server prepares room spawns.")); return; }
        if (FPlatformTime::Seconds() - EncounterStartTime > EncounterTimeoutSeconds)
        { Fail(TEXT("Room spawn/navigation preparation timed out. Check NavMesh and streamed rooms.")); return; }
        if (!IsEncounterNavigationReady()) return;
        FString Error;
        if (!PrepareEncounters(Error)) { Fail(Error); return; }
        MarkReady();
        return;
    }
    if (State != EHBDungeonRuntimeState::Loading || (CanPlanGeneration() && PCG->IsGenerating())) return;
    for (const auto& Stream : Streams)
        if (!IsValid(Stream) || !Stream->IsLevelLoaded() || !Stream->IsLevelVisible()) return;
    FString Error;
    if (GetNetMode() == NM_Client)
    {
        if (!PrepareClientGameplay(Error)) { Fail(Error); return; }
        SelectExtractionPoints();
        MarkReady();
        return;
    }
    if (!PrepareServerGameplay(Error)) { Fail(Error); return; }
    SelectExtractionPoints();
    if (!CreateDungeonNavigationBounds(Error)) { Fail(Error); return; }
    bool bHasRoomSpawns = false;
    for (const auto& Stream : Streams)
        if (const ULevel* Level = IsValid(Stream) ? Stream->GetLoadedLevel() : nullptr)
            if (UHBEncounterSubsystem::HasEncounter(Level) || UHBChestSpawnSubsystem::HasPlacement(Level))
                bHasRoomSpawns = true;
    if (!bHasRoomSpawns) { MarkReady(); return; }
    State = EHBDungeonRuntimeState::PreparingEncounters;
    EncounterStartTime = FPlatformTime::Seconds();
    Status = TEXT("Waiting for navigation and room spawns...");
}
void AHBDungeonRuntime::ClearGenerated()
{
    if (bClearingGeneration) return;
    TGuardValue<bool> ClearingGuard(bClearingGeneration, true);
    // Cancel queued Plan/Load nodes before releasing the room instances they use.
    if (IsValid(PCG) && GetNetMode() != NM_Client) PCG->CancelGeneration();
    bEncountersActive = false;
    ActiveExtractionPoints.Reset();
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Client)
	{
		if (UHBEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UHBEncounterSubsystem>())
			EncounterSubsystem->ClearGeneration(GenerationId);
		if (UHBChestSpawnSubsystem* ChestSubsystem = World->GetSubsystem<UHBChestSpawnSubsystem>())
			ChestSubsystem->ClearGeneration(GenerationId);
	}
    for (const auto& Portal : LinkedPortals) if (IsValid(Portal)) Portal->Disconnect();
    LinkedPortals.Reset(); StartPoint = nullptr;
    for (const auto& Seal : Seals) if (IsValid(Seal)) Seal->Destroy();
    Seals.Reset();
    if (IsValid(DungeonNavigationBounds)) DungeonNavigationBounds->Destroy();
    DungeonNavigationBounds = nullptr;
    for (const auto& Stream : Streams) if (IsValid(Stream))
    { Stream->SetShouldBeVisible(false); Stream->SetShouldBeLoaded(false); Stream->SetIsRequestingUnloadAndRemoval(true); }
    Streams.Reset();
    Rooms.Reset();
    ActiveSeed = 0;
    ActiveDoorSealClass = nullptr;
    ActiveDoorSealExtent = FVector::ZeroVector;
    GenerationStartTime = 0;
    EncounterStartTime = 0;
}
void AHBDungeonRuntime::Fail(const FString& Reason)
{
    if (bEndingPlay || State == EHBDungeonRuntimeState::Failed) return;
    State = EHBDungeonRuntimeState::Failed;
    Status = Reason.IsEmpty() ? TEXT("Dungeon generation failed.") : Reason;
    if (GetNetMode() != NM_Client)
    {
        if (auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>()) Source->InvalidateDungeonLayout(GenerationId, Status);
    }
    else if (SnapshotSource.IsValid() && SnapshotSource->GetDungeonSnapshot().State == EHBDungeonSnapshotState::Active
        && SnapshotSource->GetDungeonSnapshot().GenerationId == GenerationId)
    {
        // A local failure can occur after the controller already sent its one loading acknowledgement.
        // Reuse its existing owned RPC without changing player/controller implementation.
        if (auto* Player = Cast<AHBPlayerController>(GetWorld()->GetFirstPlayerController()); Player && Player->IsLocalController())
            Player->ServerReportDungeonLoaded(SnapshotSource.Get(), GenerationId, false);
    }
    ClearGenerated();
    UE_LOG(LogHBDungeonRuntime, Error, TEXT("FAILED: %s"), *Status);
    OnFailed.Broadcast(Status);
}
void AHBDungeonRuntime::ShutdownGeneration()
{
    if (bEndingPlay) return;
    const bool bServer = CanPlanGeneration();
    bEndingPlay = true;
    State = EHBDungeonRuntimeState::Idle;
    SetActorTickEnabled(false);
    if (bServer)
        if (auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>()) Source->InvalidateDungeonLayout(GenerationId);
    if (SnapshotSource.IsValid()) SnapshotSource->OnDungeonSnapshotChanged.Remove(SnapshotChangedHandle);
    SnapshotSource.Reset();
    SnapshotChangedHandle.Reset();
    ClearGenerated();
}
void AHBDungeonRuntime::EndPlay(const EEndPlayReason::Type Reason)
{
    ShutdownGeneration();
    Super::EndPlay(Reason);
}
