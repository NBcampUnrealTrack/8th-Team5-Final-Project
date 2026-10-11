#include "PCGModule/HBPortalPlacement.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBPortalSpawnPoint.h"
#include "PCGModule/HBRoomSpawnEntry.h"
#include "PCGModule/HBSpawnNavigationTargets.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    using namespace HBPortalPlacement;
    struct FPortalWorld
    {
        UWorld* World;
        TArray<ULevel*> Levels;
        TArray<FHBPrototypeRoom> Rooms;
        FPortalWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
                .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            for (int32 Index = 0; Index < 2; ++Index)
            {
                ULevel* Level = NewObject<ULevel>(World);
                Level->OwningWorld = World; Levels.Add(Level);
                FHBPrototypeRoom Room;
                Room.Id = Index; Room.Floor = Index + 1;
                const double X = Index * 10000.;
                Room.Bounds = FBox(FVector(X - 1000, -1000, -50), FVector(X + 6000, 1000, 500));
                Rooms.Add(Room);
                AddBox(FVector(X + 2500, 0, -10), FVector(3500, 1000, 10));
            }
        }
        ~FPortalWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
        UBoxComponent* AddBox(const FVector& Location, const FVector& Extent)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Box);
            Box->SetBoxExtent(Extent);
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->SetCanEverAffectNavigation(false);
            Box->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Box;
        }
        TArray<FCandidate> Candidates(int32 PerFloor) const
        {
            TArray<FCandidate> Result;
            for (int32 Floor = 1; Floor <= 2; ++Floor)
                for (int32 Index = 0; Index < PerFloor; ++Index)
                    Result.Add({{Floor - 1, FName(*FString::Printf(TEXT("P%d"), Index))}, Floor,
                        FTransform(FVector((Floor - 1) * 10000. + Index * 1500., 0, 0))});
            return Result;
        }
        bool Space(const FCandidate& Candidate, AHBDungeonPortal& Portal, FPlacement& Placement, FString& Error)
        {
            return HBDungeonRuntimeValidation::PreparePortalSpace(*World, Portal, Candidate.SpawnTransform.GetLocation(), *GetDefault<ACharacter>(),
                Rooms[Candidate.Key.RoomId].Bounds, Placement, Error);
        }
        int32 ActivePortals() const
        {
            int32 Count = 0;
            for (ULevel* Level : Levels)
                for (AActor* Actor : Level->Actors)
                    if (IsValid(Cast<AHBDungeonPortal>(Actor)) && !Actor->IsActorBeingDestroyed()) ++Count;
            return Count;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeSpaceTest, "HeavenBreach.PCGModule.PortalRuntime.Space",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeSpaceTest::RunTest(const FString&)
{
    FPortalWorld Fixture;
    auto* Portal = Fixture.World->SpawnActor<AHBDungeonPortal>();
    Portal->PortalId = TEXT("Space");
    FPlacement Placement;
    FString Error;
    TestTrue(TEXT("A floor marker prepares physical player space without NavMesh"),
        HBDungeonRuntimeValidation::PreparePortalSpace(*Fixture.World, *Portal, FVector::ZeroVector,
            *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    const FTransform OriginalArrival = Portal->Arrival->GetComponentTransform();
    TestTrue(TEXT("Actual player capsule fits a flat portal trigger and authored arrival"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    TestTrue(TEXT("Reservations contain the actual trigger and capsule arrival"), Placement.IsValid()
        && Placement.ActivationBounds.IsInsideOrOn(Portal->Trigger->GetComponentLocation())
        && Placement.ArrivalBounds.IsInsideOrOn(Portal->Arrival->GetComponentLocation()));
    auto* Wall = Fixture.AddBox(Portal->Arrival->GetComponentLocation(), FVector(10, 100, 100));
    TestFalse(TEXT("An arrival blocked by physical geometry is rejected"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    TestTrue(TEXT("Collision rejection keeps its cause"), Error.Contains(TEXT("overlaps blocking")));
    TestFalse(TEXT("Failed checks do not return stale reservations"), Placement.IsValid());
    TestTrue(TEXT("Arrival is never moved to recover a blocked portal"), Portal->Arrival->GetComponentTransform().Equals(OriginalArrival));
    Wall->GetOwner()->Destroy();
    Portal->Arrival->SetRelativeLocation(FVector(250, 0, 400));
    TestFalse(TEXT("A floating arrival is rejected without searching for another location"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    Portal->Arrival->SetWorldTransform(OriginalArrival);
    Portal->Trigger->SetBoxExtent(FVector(100, 100, 10));
    TestFalse(TEXT("A floor marker with a trigger too short for a standing pawn cannot activate"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    Portal->Trigger->SetBoxExtent(FVector(100, 100, 150));
    TestTrue(TEXT("Restoring the trigger restores physical validity"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    FHBRoomSpawnEntry Entry(Portal->Trigger->GetComponentLocation(), Portal->Arrival->GetComponentLocation());
    Entry.ReservedBounds = {Placement.ActivationBounds, Placement.ArrivalBounds};
    TestTrue(TEXT("Portal reserves bodies even if entry distance is configured to zero"),
        Entry.IntersectsReservation(FBox(FVector(240, -10, 80), FVector(260, 10, 120))));
    TestFalse(TEXT("A stacked body 1200 cm higher does not occupy this portal's reserved space"),
        Entry.IntersectsReservation(FBox(FVector(240, -10, 1280), FVector(260, 10, 1320))));
    const FBox OriginalBounds = Placement.ActivationBounds;
    Portal->Trigger->SetRelativeScale3D(FVector(1, 2, 0.75));
    TestTrue(TEXT("Positive scaled triggers use their actual physical dimensions"),
        HBDungeonRuntimeValidation::ValidatePortalSpace(*Fixture.World, *Portal, *GetDefault<ACharacter>(), Fixture.Rooms[0].Bounds, Placement, Error));
    TestTrue(TEXT("Reservation grows with actual trigger width"), Placement.ActivationBounds.GetExtent().Y > OriginalBounds.GetExtent().Y);
    Portal->Trigger->SetRelativeScale3D(FVector(1, 1, 0));
    TestFalse(TEXT("Collapsed trigger scale is invalid"), Portal->ValidateDefinition(Error));
    Portal->Trigger->SetRelativeScale3D(FVector(1, -1, 1));
    TestFalse(TEXT("Negative trigger scale is invalid"), Portal->ValidateDefinition(Error));
    Portal->Trigger->SetRelativeScale3D(FVector::OneVector);
    Portal->Arrival->SetRelativeScale3D(FVector(0.75));
    TestFalse(TEXT("Arrival still requires unit scale"), Portal->ValidateDefinition(Error));
    Portal->Arrival->SetRelativeScale3D(FVector::OneVector);
    Portal->SetActorScale3D(FVector(0.75));
    TestFalse(TEXT("Portal actor still requires unit scale"), Portal->ValidateDefinition(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimePairsTest, "HeavenBreach.PCGModule.PortalRuntime.PairsAndTravel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimePairsTest::RunTest(const FString&)
{
    FPortalWorld Fixture;
    const auto Candidates = Fixture.Candidates(3);
    TArray<FSpawnedPair> Pairs;
    FString Error;
    auto Resolve = [&](int32 RoomId) { return Fixture.Levels[RoomId]; };
    auto Validate = [&](const FCandidate& Candidate, AHBDungeonPortal& Portal, FPlacement& Placement, FString& Reason)
        { return Fixture.Space(Candidate, Portal, Placement, Reason); };
    if (!TestTrue(TEXT("Two complete pairs spawn in their own room instances"), SpawnPairs(*Fixture.World, nullptr, 7283, 2,
        AHBDungeonPortal::StaticClass(), Candidates, Resolve, Validate, Pairs, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Exactly 2N portals"), Fixture.ActivePortals(), 4);
    TestTrue(TEXT("Logical floors, ownership, unique points, reciprocal links and reservations validate"),
        HBDungeonRuntimeValidation::ValidateRandomPortalLinks(Fixture.Rooms, Fixture.Levels, 2, Pairs, Error));
    auto* Portal1 = Pairs[0].Floor1.Get();
    auto* Portal2 = Pairs[0].Floor2.Get();
    auto* Controller = Fixture.World->SpawnActor<APlayerController>();
    auto* Player = Fixture.World->SpawnActor<ACharacter>();
    Controller->Possess(Player);
    Player->SetActorLocation(Pairs[0].Placement1.ActivationLocation);
    TestFalse(TEXT("Pairs cannot teleport before runtime preparation finishes"), Portal1->TryTeleport(Player));
    for (const FSpawnedPair& Pair : Pairs) { Pair.Floor1->SetTravelEnabled(true); Pair.Floor2->SetTravelEnabled(true); }
    TestTrue(TEXT("A controlled character traverses a generated pair"), Portal1->TryTeleport(Player));
    TestTrue(TEXT("Character arrives at the BP component exactly"), Player->GetActorLocation().Equals(Portal2->Arrival->GetComponentLocation(), 0.01));
    Player->SetActorLocation(Pairs[0].Placement2.ActivationLocation);
    TestFalse(TEXT("Generated pairs preserve the immediate return guard"), Portal2->TryTeleport(Player));
    Player->SetActorLocation(Portal2->Arrival->GetComponentLocation());
    // UWorld clamps a single long frame; advance ordinary frames to exercise the actual delay.
    const double BeforeWait = Fixture.World->GetTimeSeconds();
    for (int32 Frame = 0; Frame < 8; ++Frame) Fixture.World->Tick(LEVELTICK_All, 0.1f);
    TestTrue(TEXT("Fixture advances beyond the portal reentry delay"), Fixture.World->GetTimeSeconds() - BeforeWait >= Portal2->ReentryDelay);
    Portal2->Tick(0.f);
    Player->SetActorLocation(Pairs[0].Placement2.ActivationLocation);
    TestTrue(TEXT("The same character returns after leaving and waiting"), Portal2->TryTeleport(Player));
    TestTrue(TEXT("Return honors the first endpoint arrival"), Player->GetActorLocation().Equals(Portal1->Arrival->GetComponentLocation(), 0.01));
    const auto Saved = Pairs;
    DestroyPairs(Pairs);
    TestEqual(TEXT("Generation cleanup removes every generated actor"), Fixture.ActivePortals(), 0);
    for (const auto& Pair : Saved)
        TestTrue(TEXT("Cleanup breaks reciprocal links"), (!Pair.Floor1.IsValid() || !Pair.Floor1->TargetPortal)
            && (!Pair.Floor2.IsValid() || !Pair.Floor2->TargetPortal));
    Player->Destroy(); Controller->Destroy();
    TestTrue(TEXT("A later generation can spawn cleanly after cleanup"), SpawnPairs(*Fixture.World, nullptr, 7283, 2,
        AHBDungeonPortal::StaticClass(), Candidates, Resolve, Validate, Pairs, Error));
    for (int32 Index = 0; Index < Saved.Num() && Index < Pairs.Num(); ++Index)
        TestTrue(TEXT("Identical inputs replay selection after actual actor cleanup"),
            Saved[Index].Selection.Floor1 == Pairs[Index].Selection.Floor1 && Saved[Index].Selection.Floor2 == Pairs[Index].Selection.Floor2);
    DestroyPairs(Pairs);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeRollbackTest, "HeavenBreach.PCGModule.PortalRuntime.RetryAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeRollbackTest::RunTest(const FString&)
{
    FPortalWorld Fixture;
    const auto Candidates = Fixture.Candidates(3);
    TArray<FPair> Selected;
    TArray<FSpawnedPair> Pairs;
    FString Error;
    SelectPairs(7283, 2, Candidates, Selected, Error);
    const auto* Blocked = Candidates.FindByPredicate([&](const auto& C) { return C.Key == Selected[0].Floor2; });
    auto* Wall = Fixture.AddBox(Blocked->SpawnTransform.GetLocation() + FVector(250, 0, 100), FVector(10, 100, 100));
    auto Resolve = [&](int32 RoomId) { return Fixture.Levels[RoomId]; };
    auto Validate = [&](const FCandidate& Candidate, AHBDungeonPortal& Portal, FPlacement& Placement, FString& Reason)
        { return Fixture.Space(Candidate, Portal, Placement, Reason); };
    TestTrue(TEXT("Blocked selected destination is replaced with a spare point"), SpawnPairs(*Fixture.World, nullptr, 7283, 2,
        AHBDungeonPortal::StaticClass(), Candidates, Resolve, Validate, Pairs, Error));
    TestFalse(TEXT("The blocked point never belongs to a committed pair"), Pairs.ContainsByPredicate([&](const auto& Pair) { return Pair.Selection.Floor2 == Blocked->Key; }));
    TestEqual(TEXT("Rejected candidate actors are removed"), Fixture.ActivePortals(), 4);
    DestroyPairs(Pairs); Wall->GetOwner()->Destroy();
    const auto Limited = Fixture.Candidates(2);
    SelectPairs(7283, 2, Limited, Selected, Error);
    const FPointKey Missing = Selected[1].Floor2;
    TestFalse(TEXT("A missing endpoint in a later pair rolls back the entire transaction"), SpawnPairs(*Fixture.World, nullptr, 7283, 2,
        AHBDungeonPortal::StaticClass(), Limited, Resolve,
        [&](const FCandidate& Candidate, AHBDungeonPortal& Portal, FPlacement& Placement, FString& Reason)
        {
            if (Candidate.Key == Missing) { Reason = TEXT("Injected missing endpoint"); return false; }
            return Fixture.Space(Candidate, Portal, Placement, Reason);
        }, Pairs, Error));
    TestTrue(TEXT("Rollback diagnoses the previously prepared pair and original point failure"),
        Error.Contains(TEXT("prepared=1")) && Error.Contains(TEXT("Injected missing endpoint")));
    TestTrue(TEXT("Failure returns no partial pairs"), Pairs.IsEmpty());
    TestEqual(TEXT("No half pair or earlier pair leaks after failure"), Fixture.ActivePortals(), 0);
    auto Overlapping = Fixture.Candidates(2);
    Overlapping[1].SpawnTransform = Overlapping[0].SpawnTransform;
    TestFalse(TEXT("Separate IDs cannot place two portals on overlapping activation spaces"), SpawnPairs(*Fixture.World, nullptr, 7283, 2,
        AHBDungeonPortal::StaticClass(), Overlapping, Resolve, Validate, Pairs, Error));
    TestEqual(TEXT("Reservation failure also rolls back all actors"), Fixture.ActivePortals(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeCandidatesTest, "HeavenBreach.PCGModule.PortalRuntime.LoadedPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeCandidatesTest::RunTest(const FString&)
{
    FPortalWorld Fixture;
    const auto Candidates = Fixture.Candidates(1);
    TArray<AHBPortalSpawnPoint*> Points;
    for (const auto& Candidate : Candidates)
    {
        FActorSpawnParameters Params; Params.OverrideLevel = Fixture.Levels[Candidate.Key.RoomId];
        auto* Point = Fixture.World->SpawnActor<AHBPortalSpawnPoint>(AHBPortalSpawnPoint::StaticClass(), Candidate.SpawnTransform, Params);
        Point->PointId = Candidate.Key.PointId; Points.Add(Point);
    }
    FString Error;
    TestTrue(TEXT("Repeated point ID in distinct loaded rooms stays independent"),
        HBDungeonRuntimeValidation::ValidatePortalCandidates(Fixture.Rooms, Fixture.Levels, Candidates, Error));
    Points[1]->AddActorWorldOffset(FVector(10, 0, 0));
    TestFalse(TEXT("A moved loaded point cannot silently differ from its catalog transform"),
        HBDungeonRuntimeValidation::ValidatePortalCandidates(Fixture.Rooms, Fixture.Levels, Candidates, Error));
    Points[1]->AddActorWorldOffset(FVector(-10, 0, 0));
    Points[0]->Destroy();
    TestFalse(TEXT("Missing loaded points are rejected before spawning"),
        HBDungeonRuntimeValidation::ValidatePortalCandidates(Fixture.Rooms, Fixture.Levels, Candidates, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeBlueprintTest, "HeavenBreach.PCGModule.PortalRuntime.Blueprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeBlueprintTest::RunTest(const FString&)
{
    UClass* PortalClass = LoadClass<AHBDungeonPortal>(nullptr,
        TEXT("/Game/Levels/Test_CW/Prototype/Blueprints/BP_HBDungeonPortal.BP_HBDungeonPortal_C"));
    if (!TestNotNull(TEXT("Current portal visual Blueprint loads"), PortalClass)) return false;
    const auto* Defaults = PortalClass->GetDefaultObject<AHBDungeonPortal>();
    AddInfo(FString::Printf(TEXT("Portal BP default scales: Actor=%s Trigger=%s Arrival=%s"),
        *Defaults->GetActorScale3D().ToString(), *Defaults->Trigger->GetRelativeScale3D().ToString(), *Defaults->Arrival->GetRelativeScale3D().ToString()));
    FPortalWorld Fixture;
    TArray<FSpawnedPair> Pairs;
    FString Error;
    auto Resolve = [&](int32 RoomId) { return Fixture.Levels[RoomId]; };
    auto Validate = [&](const FCandidate& Candidate, AHBDungeonPortal& Portal, FPlacement& Placement, FString& Reason)
        {
            AddInfo(FString::Printf(TEXT("Constructed portal BP: ActorScale=%s TriggerScale=%s ArrivalScale=%s Arrival=%s"),
                *Portal.GetActorScale3D().ToString(), *Portal.Trigger->GetComponentScale().ToString(),
                *Portal.Arrival->GetComponentScale().ToString(), *Portal.Arrival->GetRelativeLocation().ToString()));
            return Fixture.Space(Candidate, Portal, Placement, Reason);
        };
    auto Candidates = Fixture.Candidates(1);
    for (auto& Candidate : Candidates) Candidate.SpawnTransform.SetRotation(FRotator(0, 90, 0).Quaternion());
    const bool bSpawned = SpawnPairs(*Fixture.World, nullptr, 7283, 1, PortalClass, Candidates, Resolve, Validate, Pairs, Error);
    TestTrue(TEXT("Current portal BP spawns safely from rotated floor markers without BP edits or NavMesh"), bSpawned);
    if (!bSpawned) AddError(Error);
    if (bSpawned)
        for (const auto& Pair : Pairs)
        {
            for (AHBDungeonPortal* Portal : {Pair.Floor1.Get(), Pair.Floor2.Get()})
            {
                TestTrue(TEXT("Actual BP trigger bottom is two cm above the marker floor"),
                    FMath::IsNearlyEqual(Portal->Trigger->Bounds.GetBox().Min.Z, 2., 0.01));
                TestTrue(TEXT("Arrival height uses the player's actual capsule rather than the BP's old Z offset"),
                    FMath::IsNearlyEqual(Portal->Arrival->GetComponentLocation().Z,
                        GetDefault<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2., 0.01));
                const FVector ExpectedXY = Portal->GetActorTransform().TransformPosition(Defaults->Arrival->GetRelativeLocation());
                TestTrue(TEXT("Grounding preserves the rotated BP arrival XY and facing"),
                    FVector::Dist2D(ExpectedXY, Portal->Arrival->GetComponentLocation()) < 0.01
                    && Portal->GetActorQuat().Equals(FRotator(0, 90, 0).Quaternion(), 0.0001));
            }
            TestFalse(TEXT("Generated portal visuals cannot block their own trigger"), Pair.Floor1->GetActorEnableCollision() || Pair.Floor2->GetActorEnableCollision());
        }
    DestroyPairs(Pairs);
    return true;
}
#endif

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEncounterComponent.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestPlacementComponent.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "AI/EnemyCharacterBase.h"
#include "Loot/BoxActor.h"
namespace HBEntryPathTestFixture
{
    void Queue(FAutomationTestBase* Test, TFunction<void(UWorld&, const FBox&, const TArray<FVector>&)> Verify);
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeGroundingTest, "HeavenBreach.PCGModule.PortalRuntime.Grounding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeGroundingTest::RunTest(const FString&)
{
    FPortalWorld Fixture;
    TestTrue(TEXT("The physical portal test world has no navigation system"), Fixture.World->GetNavigationSystem() == nullptr);
    auto* Portal = Fixture.World->SpawnActor<AHBDungeonPortal>();
    Portal->PortalId = TEXT("Grounding");
    auto* PlayerDefaults = Fixture.World->SpawnActor<ACharacter>();
    PlayerDefaults->SetActorEnableCollision(false);
    PlayerDefaults->GetCapsuleComponent()->SetCapsuleSize(60, 120);
    Portal->Trigger->SetRelativeLocation(FVector(0, 0, 30));
    Portal->Trigger->SetRelativeScale3D(FVector(1, 1, 1.5));
    FPlacement Placement;
    FString Error;
    const FVector ArrivalXY = Portal->Arrival->GetComponentLocation();
    auto Ground = [&](const FVector& Point)
    {
        return HBDungeonRuntimeValidation::PreparePortalSpace(*Fixture.World, *Portal, Point, *PlayerDefaults,
            Fixture.Rooms[0].Bounds, Placement, Error);
    };
    TestTrue(TEXT("Floor markers handle BP trigger offsets and scale with a larger actual player capsule"), Ground(FVector::ZeroVector));
    TestTrue(TEXT("Constructed trigger bottom stays just above the floor"), FMath::IsNearlyEqual(Portal->Trigger->Bounds.GetBox().Min.Z, 2., 0.01));
    TestTrue(TEXT("Arrival uses the supplied player half height"), FMath::IsNearlyEqual(Portal->Arrival->GetComponentLocation().Z, 122., 0.01));
    TestTrue(TEXT("Height correction never searches for a different arrival XY"), FVector::Dist2D(ArrivalXY, Portal->Arrival->GetComponentLocation()) < 0.01);

    auto* Platform = Fixture.AddBox(FVector(ArrivalXY.X, ArrivalXY.Y, 30), FVector(80, 80, 10));
    TestTrue(TEXT("Arrival follows its own nearby raised floor instead of the source floor"), Ground(FVector(0, 0, 10)));
    TestTrue(TEXT("Destination feet clear the raised floor by two cm"), FMath::IsNearlyEqual(Portal->Arrival->GetComponentLocation().Z, 162., 0.01));
    TestTrue(TEXT("A marker slightly above the floor is still aligned to physical ground"), FMath::IsNearlyEqual(Portal->Trigger->Bounds.GetBox().Min.Z, 2., 0.01));
    TestFalse(TEXT("A marker far above physical ground cannot snap to a lower storey"), Ground(FVector(0, 0, 300)));
    TestFalse(TEXT("Rejected grounding clears stale reservations"), Placement.IsValid());

    const FTransform Arrival = Portal->Arrival->GetComponentTransform();
    Portal->Arrival->SetWorldLocation(FVector(8000, 0, 162));
    TestFalse(TEXT("An arrival without supporting floor is rejected without lateral search"), Ground(FVector::ZeroVector));
    Portal->Arrival->SetWorldTransform(Arrival);
    auto* Wall = Fixture.AddBox(FVector(0, 0, 70), FVector(10, 60, 30));
    TestFalse(TEXT("An obstacle inside the trigger cannot be mistaken for its floor"), Ground(FVector::ZeroVector));
    TestTrue(TEXT("Physical activation collision is still diagnosed without NavMesh"), Error.Contains(TEXT("overlaps blocking")));
    Wall->GetOwner()->Destroy();
    Portal->Trigger->SetBoxExtent(FVector(100, 100, 10));
    TestFalse(TEXT("Grounding cannot make an undersized trigger contain the standing player"), Ground(FVector::ZeroVector));
    Portal->Destroy(); PlayerDefaults->Destroy(); Platform->GetOwner()->Destroy();

    Fixture.AddBox(FVector(2500, 0, 1190), FVector(3500, 1000, 10));
    Fixture.Rooms[1].Bounds = FBox(FVector(-1000, -1000, 1150), FVector(6000, 1000, 1700));
    auto Candidates = Fixture.Candidates(1);
    Candidates[1].SpawnTransform.SetLocation(FVector(0, 0, 1200));
    TArray<FSpawnedPair> Pairs;
    const bool bSpawned = SpawnPairs(*Fixture.World, nullptr, 7283, 1, AHBDungeonPortal::StaticClass(), Candidates,
        [&](int32 RoomId) { return Fixture.Levels[RoomId]; },
        [&](const FCandidate& Candidate, AHBDungeonPortal& Spawned, FPlacement& Space, FString& Reason)
            { return Fixture.Space(Candidate, Spawned, Space, Reason); }, Pairs, Error);
    TestTrue(TEXT("Pairs on stacked floors generate without a navigation system"), bSpawned);
    if (!bSpawned) AddError(Error);
    if (bSpawned)
    {
        TestTrue(TEXT("Each endpoint stays on the floor of its own marker"),
            FMath::IsNearlyEqual(Pairs[0].Floor1->Trigger->Bounds.GetBox().Min.Z, 2., 0.01)
            && FMath::IsNearlyEqual(Pairs[0].Floor2->Trigger->Bounds.GetBox().Min.Z, 1202., 0.01));
        TestTrue(TEXT("Stacked portals keep nonoverlapping 3D reservations and correct floor links"),
            HBDungeonRuntimeValidation::ValidateRandomPortalLinks(Fixture.Rooms, Fixture.Levels, 1, Pairs, Error));
    }
    DestroyPairs(Pairs);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimePathTargetsTest, "HeavenBreach.PCGModule.PortalRuntime.PathTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimePathTargetsTest::RunTest(const FString&)
{
    FHBRoomSpawnEntry Portal(FVector(0, 0, 150), FVector(250, 0, 90));
    Portal.bUseForNavigation = false;
    Portal.ReservedBounds = {FBox(FVector(200, -50, 0), FVector(300, 50, 180))};
    FHBRoomSpawnEntry Door(FVector(1000, 0, 0), FVector(900, 0, 0));
    TArray<FHBSpawnNavigationTarget> Targets;
    FString Error;
    TestTrue(TEXT("Entries with a portal still resolve normal path targets"),
        HBSpawnNavigationTargets::Resolve(NAME_None, {}, {Portal, Door}, Targets, Error));
    TestEqual(TEXT("Portal arrival is excluded from NPC and chest navigation targets"), Targets.Num(), 1);
    if (Targets.Num() == 1)
        TestTrue(TEXT("Door target keeps its original entry index and navigation location"),
            Targets[0].EntryIndex == 1 && Targets[0].Location.Equals(Door.NavigationLocation));
    TestTrue(TEXT("Disabling path use keeps the portal's physical reservation"),
        Portal.IsFinite() && Portal.IntersectsReservation(FBox(FVector(240, -10, 80), FVector(260, 10, 100))));
    TestTrue(TEXT("Portal-only entries resolve without inventing a navigation target"),
        HBSpawnNavigationTargets::Resolve(NAME_None, {}, {Portal}, Targets, Error) && Targets.IsEmpty());
    TMap<FName, FVector> Anchors; Anchors.Add(TEXT("Local"), FVector(500, 0, 0));
    TestTrue(TEXT("Explicit LD anchors remain usable independently of portal entries"),
        HBSpawnNavigationTargets::Resolve(TEXT("Local"), Anchors, {Portal}, Targets, Error)
        && Targets.Num() == 1 && Targets[0].AnchorId == TEXT("Local"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalRuntimeReservationsTest, "HeavenBreach.PCGModule.PortalRuntime.SpawnReservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalRuntimeReservationsTest::RunTest(const FString&)
{
    HBEntryPathTestFixture::Queue(this, [this](UWorld& World, const FBox& Bounds, const TArray<FVector>& Locations)
    {
        auto* Portal = World.SpawnActor<AHBDungeonPortal>();
        Portal->PortalId = TEXT("Reservation");
        Portal->SetActorLocation(Locations[0] - FVector(0, 0, 0.4));
        FPlacement Placement;
        FString Error;
        if (!TestTrue(TEXT("Reservations come from an actual portal and player capsule"),
            HBDungeonRuntimeValidation::PreparePortalSpace(World, *Portal, Locations[0] - FVector(0, 0, 0.4),
                *GetDefault<ACharacter>(), Bounds, Placement, Error))) return;
        FHBRoomSpawnEntry Entry(Locations[1]);
        FHBRoomSpawnEntry PortalEntry(Portal->Trigger->GetComponentLocation(), Portal->Arrival->GetComponentLocation());
        PortalEntry.bUseForNavigation = false;
        const FVector Marker = Portal->Arrival->GetComponentLocation()
            - FVector(0, 0, GetDefault<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.);
        auto* EnemySetup = World.SpawnActor<AHBEncounterSetupActor>();
        EnemySetup->Encounter->MinEntryDistance = 0;
        auto* EnemyPoint = World.SpawnActor<AHBEnemySpawnPoint>();
        EnemyPoint->PointId = TEXT("ReservedEnemy"); EnemyPoint->GroupId = TEXT("Field");
        EnemyPoint->bRequired = true; EnemyPoint->bAllowPositionCorrection = false;
        EnemyPoint->SetActorLocation(Marker);
        FHBEncounterGroup EnemyGroup;
        EnemyGroup.GroupId = EnemyPoint->GroupId; EnemyGroup.MinCount = EnemyGroup.MaxCount = 1;
        EnemyGroup.Monsters = {{AEnemyCharacterBase::StaticClass(), 1}};
        EnemySetup->Encounter->Groups = {EnemyGroup};
        auto* Enemies = World.GetSubsystem<UHBEncounterSubsystem>();
        TestTrue(TEXT("The enemy point is otherwise valid with zero entry distance"),
            Enemies->StartEncounterWithEntries(900, 7283, 0, World.PersistentLevel, Bounds, {Entry, PortalEntry}, Error, true));
        Enemies->ClearGeneration(900);
        PortalEntry.ReservedBounds = {Placement.ActivationBounds, Placement.ArrivalBounds};
        TestFalse(TEXT("An actual enemy capsule cannot occupy the portal arrival reservation"),
            Enemies->StartEncounterWithEntries(901, 7283, 0, World.PersistentLevel, Bounds, {Entry, PortalEntry}, Error, true));
        TestTrue(TEXT("Enemy reservation failure reports the specific exclusion cause"),
            Error.Contains(TEXT("Cause=ExcludedArea")) && Error.Contains(TEXT("reserved portal")));
        Enemies->ClearGeneration(901); EnemyPoint->Destroy(); EnemySetup->Destroy();
        UClass* ChestClass = LoadClass<ABoxActor>(nullptr, TEXT("/Game/Loot/Blueprints/BP_Box_Rare.BP_Box_Rare_C"));
        if (!TestNotNull(TEXT("Reservation test uses the actual chest mesh"), ChestClass)) return;
        auto* ChestSetup = World.SpawnActor<AHBChestSetupActor>();
        ChestSetup->Placement->MinEntryDistance = 0;
        auto* ChestPoint = World.SpawnActor<AHBChestSpawnPoint>();
        ChestPoint->PointId = TEXT("ReservedChest"); ChestPoint->GroupId = TEXT("Field");
        ChestPoint->bRequired = true; ChestPoint->bAllowPositionCorrection = false;
        ChestPoint->SetActorLocation(Marker);
        FHBChestPlacementGroup ChestGroup;
        ChestGroup.GroupId = ChestPoint->GroupId; ChestGroup.MinCount = ChestGroup.MaxCount = 1;
        ChestGroup.Chests = {{ChestClass, 1}};
        ChestSetup->Placement->Groups = {ChestGroup};
        auto* Chests = World.GetSubsystem<UHBChestSpawnSubsystem>();
        PortalEntry.ReservedBounds.Reset();
        TestTrue(TEXT("The chest point is otherwise valid with zero entry distance"),
            Chests->StartPlacementWithEntries(902, 7283, 0, World.PersistentLevel, Bounds, {Entry, PortalEntry}, Error));
        Chests->ClearGeneration(902);
        PortalEntry.ReservedBounds = {Placement.ActivationBounds, Placement.ArrivalBounds};
        TestFalse(TEXT("An actual chest mesh cannot occupy the portal arrival reservation"),
            Chests->StartPlacementWithEntries(903, 7283, 0, World.PersistentLevel, Bounds, {Entry, PortalEntry}, Error));
        TestTrue(TEXT("Chest reservation failure reports the specific exclusion cause"),
            Error.Contains(TEXT("Cause=ExcludedArea")) && Error.Contains(TEXT("reserved portal")));
        Chests->ClearGeneration(903);
        ChestPoint->Destroy(); ChestSetup->Destroy(); Portal->Destroy();
    });
    return true;
}
#endif
