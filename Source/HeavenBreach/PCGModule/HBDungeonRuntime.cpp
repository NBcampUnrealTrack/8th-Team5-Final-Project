#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
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
    if (!HasAuthority() || GetNetMode() != NM_Standalone)
    { Fail(TEXT("Gameplay dungeon currently supports single-player Standalone/one-player PIE only.")); return; }
    int32 Count = 0;
    for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It) ++Count;
    if (Count != 1) { Fail(TEXT("Place exactly one gameplay dungeon generator.")); return; }
    for (TActorIterator<AHBPCGPreviewGenerator> It(GetWorld()); It; ++It)
    { Fail(TEXT("Remove the Preview generator from the gameplay map.")); return; }
    if (!Graph) { Fail(TEXT("Select the dungeon Plan -> Load PCG graph.")); return; }
    if (!DoorSealClass || DoorSealExtent.ContainsNaN() || DoorSealExtent.GetMin() <= 0 || SpawnTag.IsNone())
    { Fail(TEXT("Configure a door seal class, positive extent and spawn tag.")); return; }
    if (!FMath::IsFinite(LoadTimeoutSeconds) || LoadTimeoutSeconds <= 0)
    { Fail(TEXT("Load timeout must be positive and finite.")); return; }
    if (!FMath::IsFinite(EncounterTimeoutSeconds) || EncounterTimeoutSeconds <= 0)
    { Fail(TEXT("Encounter timeout must be positive and finite.")); return; }
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
    if (State != EHBDungeonRuntimeState::Generating || !Rooms.IsEmpty()) return false;
    FString Error;
    if (!HBDungeonRuleLayout::Generate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error))
    { Fail(Error); return false; }
    return true;
}
void AHBDungeonRuntime::MaterializeLayout(int32 PointCount)
{
    if (State != EHBDungeonRuntimeState::Generating || !Streams.IsEmpty()) return;
    FString Error;
    if (PointCount != Rooms.Num() || Rooms.IsEmpty())
    { Fail(TEXT("PCG point count does not match the planned layout.")); return; }
    if (!HBDungeonRuleLayout::Validate(ModuleCatalog, ActiveSettings, ActiveRules, Rooms, Error))
    { Fail(Error); return; }
    State = EHBDungeonRuntimeState::Loading;
    for (const auto& Room : Rooms)
    {
        bool bSuccess = false;
        auto* Stream = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, Room.ModuleMap, Room.Transform,
            bSuccess, FString::Printf(TEXT("HBDungeon_%s_R%d"), *GetName(), Room.Id));
        if (!bSuccess || !Stream) { Fail(FString::Printf(TEXT("Failed to load room %d."), Room.Id)); return; }
        Streams.Add(Stream);
    }
    Status = TEXT("Loading dungeon rooms...");
}
bool AHBDungeonRuntime::PrepareGameplay(FString& Error)
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
				|| Connectors[I].Role != Module->Connectors[I].Role
                || !Connectors[I].LocalTransform.Equals(Module->Connectors[I].LocalTransform, 0.1))
            { Error = TEXT("Loaded connector geometry differs from the plan."); return false; }
        for (int32 I = 0; I < Occupancy.Num(); ++I)
            if (!Occupancy[I].LocalTransform.Equals(Module->Occupancy[I].LocalTransform, 0.1)
                || !Occupancy[I].Extent.Equals(Module->Occupancy[I].Extent, 0.1))
            { Error = TEXT("Loaded occupancy geometry differs from the plan."); return false; }
        for (const auto& Connector : Module->Connectors)
        {
            if (!Room.UnusedConnectorIds.Contains(Connector.ConnectorId)) continue;
            FTransform Transform = Connector.LocalTransform * Room.Transform;
            Transform.AddToTranslation(Transform.GetRotation().RotateVector(FVector(0, 0, DoorSealExtent.Z)));
            FActorSpawnParameters Params; Params.Owner = this;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Seal = GetWorld()->SpawnActor<AHBDungeonDoorSeal>(DoorSealClass, Transform, Params);
            if (!Seal) { Error = TEXT("Could not create unused-door seal."); return false; }
            Seal->SetExtent(DoorSealExtent); Seals.Add(Seal);
        }
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
ULevel* AHBDungeonRuntime::GetLoadedRoomLevel(int32 RoomId) const
{
	return Streams.IsValidIndex(RoomId) && IsValid(Streams[RoomId]) ? Streams[RoomId]->GetLoadedLevel() : nullptr;
}
bool AHBDungeonRuntime::CreateDungeonNavigationBounds(FString& Error)
{
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
        if (!Level || !UHBEncounterSubsystem::HasEncounter(Level)) continue;
        bool bProjectedPoint = false;
        for (AActor* Actor : Level->Actors)
        {
            const AHBEnemySpawnPoint* Point = Cast<AHBEnemySpawnPoint>(Actor);
            if (!IsValid(Point) || Point->GetLevel() != Level) continue;
            FNavLocation OnNav;
            if (Nav->ProjectPointToNavigation(Point->GetActorLocation(), OnNav, FVector(150, 150, 350)))
            { bProjectedPoint = true; break; }
        }
        if (!bProjectedPoint) return false;
    }
    return true;
}
bool AHBDungeonRuntime::PrepareEncounters(FString& Error)
{
	UHBEncounterSubsystem* EncounterSubsystem = GetWorld()->GetSubsystem<UHBEncounterSubsystem>();
	if (!EncounterSubsystem) { Error = TEXT("Encounter subsystem is unavailable."); return false; }
    for (const auto& Room : Rooms)
    {
        if (!Streams.IsValidIndex(Room.Id) || !IsValid(Streams[Room.Id]))
        { Error = FString::Printf(TEXT("Missing streamed room %d for encounter preparation."), Room.Id); return false; }
        ULevel* Level = Streams[Room.Id]->GetLoadedLevel();
        if (!Level) { Error = FString::Printf(TEXT("Room %d was unloaded before encounter preparation."), Room.Id); return false; }
        AHBDungeonModuleDefinition* Definition = nullptr;
        for (AActor* Actor : Level->Actors)
            if (auto* Candidate = Cast<AHBDungeonModuleDefinition>(Actor)) { Definition = Candidate; break; }
        if (!Definition) { Error = FString::Printf(TEXT("Room %d has no module definition."), Room.Id); return false; }
        if (!UHBEncounterSubsystem::HasEncounter(Level)) continue;
        TArray<FVector> Entries;
        for (AActor* Actor : Level->Actors)
        {
            if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal) && IsValid(Portal->TargetPortal))
                Entries.Add(Portal->Arrival->GetComponentLocation());
            if (const auto* Start = Cast<APlayerStart>(Actor); IsValid(Start) && Start->PlayerStartTag == SpawnTag)
                Entries.Add(Start->GetActorLocation());
        }
        for (AActor* Actor : Level->Actors)
            if (IsValid(Actor))
            {
                TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(Actor);
                for (const auto* Connector : Connectors)
                    if (Room.UsedConnectorIds.Contains(Connector->GetEffectiveConnectorId()))
                        Entries.Add(Connector->GetComponentLocation());
            }
        if (!EncounterSubsystem->StartEncounter(GenerationId, ActiveSeed, Room.Id, Level, Room.Bounds, Entries, Error))
        {
            Error = FString::Printf(TEXT("Room %d encounter: %s"), Room.Id, *Error);
            return false;
        }
    }
    return true;
}
void AHBDungeonRuntime::MarkReady()
{
    State = EHBDungeonRuntimeState::Ready;
    Status = TEXT("READY - gameplay dungeon and encounters prepared.");
    UE_LOG(LogHBDungeonRuntime, Display, TEXT("%s Seed=%d Rooms=%d Seals=%d"), *Status, ActiveSeed, Rooms.Num(), Seals.Num());
    OnReady.Broadcast();
}
void AHBDungeonRuntime::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (State != EHBDungeonRuntimeState::Generating && State != EHBDungeonRuntimeState::Loading
        && State != EHBDungeonRuntimeState::PreparingEncounters) return;
    if (State != EHBDungeonRuntimeState::PreparingEncounters && FPlatformTime::Seconds() - GenerationStartTime > LoadTimeoutSeconds)
    { Fail(TEXT("Dungeon generation/loading timed out. Check graph and source maps.")); return; }
    if (State == EHBDungeonRuntimeState::PreparingEncounters)
    {
        if (FPlatformTime::Seconds() - EncounterStartTime > EncounterTimeoutSeconds)
        { Fail(TEXT("Room encounter/navigation preparation timed out. Check NavMesh and streamed rooms.")); return; }
        if (!IsEncounterNavigationReady()) return;
        FString Error;
        if (!PrepareEncounters(Error)) { Fail(Error); return; }
        MarkReady();
        return;
    }
    if (State != EHBDungeonRuntimeState::Loading || PCG->IsGenerating()) return;
    for (const auto& Stream : Streams)
        if (!IsValid(Stream) || !Stream->IsLevelLoaded() || !Stream->IsLevelVisible()) return;
    FString Error;
    if (!PrepareGameplay(Error)) { Fail(Error); return; }
    if (!CreateDungeonNavigationBounds(Error)) { Fail(Error); return; }
    bool bHasEncounters = false;
    for (const auto& Stream : Streams)
        if (const ULevel* Level = IsValid(Stream) ? Stream->GetLoadedLevel() : nullptr)
            if (UHBEncounterSubsystem::HasEncounter(Level)) bHasEncounters = true;
    if (!bHasEncounters) { MarkReady(); return; }
    State = EHBDungeonRuntimeState::PreparingEncounters;
    EncounterStartTime = FPlatformTime::Seconds();
    Status = TEXT("Waiting for navigation and room encounters...");
}
void AHBDungeonRuntime::ClearGenerated()
{
	if (UWorld* World = GetWorld())
		if (UHBEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UHBEncounterSubsystem>())
			EncounterSubsystem->ClearGeneration(GenerationId);
    for (const auto& Portal : LinkedPortals) if (IsValid(Portal)) Portal->Disconnect();
    LinkedPortals.Reset(); StartPoint = nullptr;
    for (const auto& Seal : Seals) if (IsValid(Seal)) Seal->Destroy();
    Seals.Reset();
    if (IsValid(DungeonNavigationBounds)) DungeonNavigationBounds->Destroy();
    DungeonNavigationBounds = nullptr;
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
    State = EHBDungeonRuntimeState::Idle;
    ClearGenerated();
    Super::EndPlay(Reason);
}
