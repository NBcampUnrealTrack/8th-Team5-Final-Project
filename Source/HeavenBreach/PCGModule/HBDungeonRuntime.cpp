#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "PCGComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "CollisionQueryParams.h"
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
    Status = bValid ? TEXT("Catalog validated. Configure transitions and DungeonSpawn PlayerStarts.") : FString::Join(CatalogErrors, TEXT("\n"));
}
void AHBDungeonRuntime::StartGeneration()
{
    if (State != EHBDungeonRuntimeState::Idle) return;
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    // Non-replicated local actors can have authority on a client; check world net mode too.
    if (!CanPlanGeneration()) return;
    int32 Count = 0;
    for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It) ++Count;
    if (Count != 1) { Fail(TEXT("Place exactly one gameplay dungeon generator.")); return; }
    for (TActorIterator<AHBPCGPreviewGenerator> It(GetWorld()); It; ++It)
    { Fail(TEXT("Remove the Preview generator from the gameplay map.")); return; }
    if (!Graph) { Fail(TEXT("Select the dungeon Plan -> Load PCG graph.")); return; }
    FString SettingsError;
    if (!ValidateLocalSettings(SettingsError)) { Fail(SettingsError); return; }
    if (!HBDungeonModuleCatalog::Build(ModuleMaps, ModuleCatalog, CatalogErrors))
    { Fail(FString::Join(CatalogErrors, TEXT("\n"))); return; }
    ActiveSeed = bRandomSeedOnStart ? static_cast<int32>(FGuid::NewGuid().A & 0x7fffffff) : Seed;
    ++GenerationId;
    ActiveRules = DungeonRules;
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
bool AHBDungeonRuntime::PlanLayout()
{
    if (!CanPlanGeneration() || State != EHBDungeonRuntimeState::Generating || !Rooms.IsEmpty()) return false;
    FString Error;
    if (!HBDungeonRuleLayout::Generate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error))
    { Fail(Error); return false; }
    if (!HBDungeonRuleLayout::Validate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error) ||
        !HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error))
    { Fail(Error); return false; }
    auto* GameState = GetWorld()->GetGameState<AHeavenBreachGameState>();
    if (!GameState || !GameState->PublishDungeonLayout(GenerationId, ActiveSeed, Rooms))
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
    return GetWorld() && GetWorld()->IsGameWorld() && HasAuthority() &&
        (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer);
}

bool AHBDungeonRuntime::ValidateLocalSettings(FString& Error) const
{
    if (!DoorSealClass || DoorSealExtent.ContainsNaN() || DoorSealExtent.GetMin() <= 0 || SpawnTag.IsNone())
    { Error = TEXT("Configure a door seal class, positive extent and spawn tag."); return false; }
    if (!FMath::IsFinite(LoadTimeoutSeconds) || LoadTimeoutSeconds <= 0)
    { Error = TEXT("Load timeout must be positive and finite."); return false; }
    return true;
}

bool AHBDungeonRuntime::ApplyServerLayout(int32 InGenerationId, int32 InSeed, const TArray<FHBPrototypeRoom>& InRooms)
{
    if (!GetWorld() || !GetWorld()->IsGameWorld() || GetNetMode() != NM_Client || InGenerationId <= 0) return false;
    if (InGenerationId < GenerationId) return false;
    if (InGenerationId == GenerationId) return State == EHBDungeonRuntimeState::Loading || State == EHBDungeonRuntimeState::Ready;
    FString Error;
    // Reject malformed input without discarding a currently loaded generation.
    if (!ValidateLocalSettings(Error) || !HBDungeonRuntimeValidation::ValidateLayoutForLoading(InRooms, Error))
    {
        UE_LOG(LogHBDungeonRuntime, Warning, TEXT("Rejected server layout: %s"), *Error);
        return false;
    }
    ClearGenerated();
    GenerationId = InGenerationId;
    ActiveSeed = InSeed;
    Rooms = InRooms;
    ModuleCatalog.Reset(); // Clients never build or use the planner's source catalog.
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
        // Shared generation/room IDs, never local actor names, identify level instances.
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
    for (FName Id : Room.UnusedConnectorIds)
    {
        const auto* Connector = Connectors.FindByPredicate([Id](const auto& C) { return C.ConnectorId == Id; });
        if (!Connector) { Error = TEXT("Layout references a missing unused connector."); return false; }
        FTransform Transform = Connector->LocalTransform * Room.Transform;
        Transform.AddToTranslation(Transform.GetRotation().RotateVector(FVector(0, 0, DoorSealExtent.Z)));
        FActorSpawnParameters Params; Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Seal = GetWorld()->SpawnActor<AHBDungeonDoorSeal>(DoorSealClass, Transform, Params);
        if (!Seal) { Error = TEXT("Could not create unused-door seal."); return false; }
        Seal->SetExtent(DoorSealExtent); Seals.Add(Seal);
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
            if (auto* D = Cast<AHBDungeonModuleDefinition>(Actor))
            {
                if (Definition) { Error = TEXT("Multiple module definitions in a local room."); return false; }
                Definition = D;
            }
        TArray<FHBDungeonConnectorDefinition> Connectors;
        TArray<FHBDungeonOccupancyDefinition> Occupancy;
        TArray<FHBDungeonModuleIssue> Issues;
        if (!Definition || Definition->ModuleId != Room.ModuleId ||
            !Definition->ReadModuleGeometry(Room.Transform, Connectors, Occupancy, Issues))
        { Error = TEXT("Loaded client room does not match the authoritative module."); return false; }
        if (!SpawnDoorSeals(Room, Connectors, Error)) return false;
    }
    // No GameMode, spawn selection, rule generation or authority-owned portal linking here.
    return true;
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
            || !Definition->ReadModuleGeometry(Room.Transform, Connectors, Occupancy, Issues)
            || Connectors.Num() != Module->Connectors.Num() || Occupancy.Num() != Module->Occupancy.Num())
        { Error = FString::Printf(TEXT("Room %d no longer matches the source module."), Room.Id); return false; }
        for (int32 I = 0; I < Connectors.Num(); ++I)
            if (Connectors[I].ConnectorId != Module->Connectors[I].ConnectorId
                || !Connectors[I].LocalTransform.Equals(Module->Connectors[I].LocalTransform, 0.1))
            { Error = TEXT("Loaded connector geometry differs from the plan."); return false; }
        for (int32 I = 0; I < Occupancy.Num(); ++I)
            if (!Occupancy[I].LocalTransform.Equals(Module->Occupancy[I].LocalTransform, 0.1)
                || !Occupancy[I].Extent.Equals(Module->Occupancy[I].Extent, 0.1))
            { Error = TEXT("Loaded occupancy geometry differs from the plan."); return false; }
        if (!SpawnDoorSeals(Room, Connectors, Error)) return false;
    }
    if (!HBDungeonRuntimeValidation::LinkPortals(Rooms, ModuleCatalog, Streams, LinkedPortals, Error)) return false;
    for (AActor* Actor : Streams[0]->GetLoadedLevel()->Actors)
        if (auto* Start = Cast<APlayerStart>(Actor); Start && Start->PlayerStartTag == SpawnTag)
        {
            if (StartPoint) { Error = TEXT("Starting room contains multiple PlayerStarts with the configured spawn tag."); return false; }
            StartPoint = Start;
        }
    if (!StartPoint) { Error = TEXT("Starting room needs one PlayerStart tagged DungeonSpawn (or the configured SpawnTag). No fallback to world origin."); return false; }
    const auto* Mode = GetWorld()->GetAuthGameMode();
    const auto* Character = Mode && Mode->DefaultPawnClass ? Cast<ACharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
    if (!Character) { Error = TEXT("Gameplay dungeon requires a Character default pawn."); return false; }
    const auto* Capsule = Character->GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HBDungeonSpawn), false);
    Query.AddIgnoredActor(StartPoint);
    if (GetWorld()->OverlapBlockingTestByProfile(StartPoint->GetActorLocation(), StartPoint->GetActorQuat(), Capsule->GetCollisionProfileName(), Shape, Query))
    { Error = TEXT("DungeonSpawn is blocked for the gameplay character capsule."); return false; }
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, StartPoint->GetActorLocation(),
        StartPoint->GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 100), ECC_WorldStatic, Query))
    { Error = TEXT("DungeonSpawn has no blocking floor beneath it."); return false; }
    return true;
}
APlayerStart* AHBDungeonRuntime::GetPlayerStart() const { return IsReady() ? StartPoint.Get() : nullptr; }
void AHBDungeonRuntime::BindSnapshotSource()
{
    auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>();
    if (SnapshotSource.Get() == Source) return;
    if (SnapshotSource.IsValid()) SnapshotSource->OnDungeonSnapshotChanged.Remove(SnapshotChangedHandle);
    SnapshotSource = Source;
    SnapshotChangedHandle.Reset();
    if (!Source) return;
    SnapshotChangedHandle = Source->OnDungeonSnapshotChanged.AddUObject(this, &AHBDungeonRuntime::ConsumeServerSnapshot);
    // RepNotify may have run before the map actor's BeginPlay.
    ConsumeServerSnapshot();
}

void AHBDungeonRuntime::ConsumeServerSnapshot()
{
    if (GetNetMode() != NM_Client || !SnapshotSource.IsValid()) return;
    const auto& Snapshot = SnapshotSource->GetDungeonSnapshot();
    if (Snapshot.GenerationId <= GenerationId) return;
    if (!ApplyServerLayout(Snapshot.GenerationId, Snapshot.Seed, Snapshot.Rooms))
        Fail(TEXT("Could not apply replicated dungeon layout."));
}

void AHBDungeonRuntime::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // GameState can arrive after the local map actor has begun play.
    if (GetNetMode() == NM_Client) BindSnapshotSource();
    if (State != EHBDungeonRuntimeState::Generating && State != EHBDungeonRuntimeState::Loading) return;
    if (FPlatformTime::Seconds() - GenerationStartTime > LoadTimeoutSeconds)
    { Fail(TEXT("Dungeon generation/loading timed out. Check graph and source maps.")); return; }
    if (State != EHBDungeonRuntimeState::Loading || (CanPlanGeneration() && PCG->IsGenerating())) return;
    for (const auto& Stream : Streams)
        if (!IsValid(Stream) || !Stream->IsLevelLoaded() || !Stream->IsLevelVisible()) return;
    FString Error;
    const bool bPrepared = GetNetMode() == NM_Client ? PrepareClientGameplay(Error) : PrepareServerGameplay(Error);
    if (!bPrepared) { Fail(Error); return; }
    State = EHBDungeonRuntimeState::Ready;
    Status = TEXT("READY - local dungeon prepared (not all-player readiness).");
    UE_LOG(LogHBDungeonRuntime, Display, TEXT("%s Seed=%d Rooms=%d Seals=%d"), *Status, ActiveSeed, Rooms.Num(), Seals.Num());
    OnReady.Broadcast();
}
void AHBDungeonRuntime::ClearGenerated()
{
    for (const auto& Portal : LinkedPortals) if (IsValid(Portal)) Portal->Disconnect();
    LinkedPortals.Reset(); StartPoint = nullptr;
    for (const auto& Seal : Seals) if (IsValid(Seal)) Seal->Destroy();
    Seals.Reset();
    for (const auto& Stream : Streams) if (IsValid(Stream))
    { Stream->SetShouldBeVisible(false); Stream->SetShouldBeLoaded(false); Stream->SetIsRequestingUnloadAndRemoval(true); }
    Streams.Reset();
}
void AHBDungeonRuntime::Fail(const FString& Reason)
{
    if (State == EHBDungeonRuntimeState::Failed) return;
    State = EHBDungeonRuntimeState::Failed; Status = Reason;
    ClearGenerated();
    UE_LOG(LogHBDungeonRuntime, Error, TEXT("FAILED: %s"), *Reason);
    OnFailed.Broadcast(Reason);
}
void AHBDungeonRuntime::EndPlay(const EEndPlayReason::Type Reason)
{
    if (SnapshotSource.IsValid()) SnapshotSource->OnDungeonSnapshotChanged.Remove(SnapshotChangedHandle);
    SnapshotSource.Reset();
    SnapshotChangedHandle.Reset();
    State = EHBDungeonRuntimeState::Idle;
    ClearGenerated();
    Super::EndPlay(Reason);
}
