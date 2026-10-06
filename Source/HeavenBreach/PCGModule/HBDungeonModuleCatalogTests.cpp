#include "PCGModule/HBDungeonModuleCatalog.h"
#include "PCGModule/HBDungeonMarkerActors.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Level.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FCatalogFixture
    {
        TArray<UWorld*> Worlds;
        UWorld* Add(FName Id, EHBPrototypeRoomRole Role, EHBDungeonModuleKind Kind = EHBDungeonModuleKind::Room)
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
            UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
            Worlds.Add(World);
            auto* Definition = World->SpawnActor<AHBDungeonModuleDefinition>();
            Definition->ModuleId = Id; Definition->ModuleKind = Kind;
            Definition->RoomInfo->RoomRole = Role;
            World->SpawnActor<AHBDungeonConnector>();
            if (Kind == EHBDungeonModuleKind::Corridor) { World->SpawnActor<AHBDungeonConnector>(); }
            auto* Box = World->SpawnActor<AHBDungeonOccupancy>();
            Box->SetActorLocation(FVector(200,300,100));
            Box->SetActorScale3D(FVector(2,3,1));
            return World;
        }
        ~FCatalogFixture()
        {
            for (UWorld* World : Worlds) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBCatalogReadTest, "HeavenBreach.PCGModule.CatalogRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBCatalogReadTest::RunTest(const FString&)
{
    FCatalogFixture Fixture;
    UWorld* Recovery = Fixture.Add(TEXT("NotNamedHeal"), EHBPrototypeRoomRole::Recovery);
    for (AActor* Actor : Recovery->PersistentLevel->Actors)
        if (auto* Definition = Cast<AHBDungeonModuleDefinition>(Actor)) Definition->RoomInfo->bIsStartCandidate = true;
    Recovery->SpawnActor<APlayerStart>();
    UWorld* Corridor = Fixture.Add(TEXT("Stairs"), EHBPrototypeRoomRole::Unassigned, EHBDungeonModuleKind::Corridor);
    TArray<FHBModuleCatalogEntry> Catalog;
    TArray<FString> Errors;
    TestTrue(TEXT("Read role from map component, not name or array index"), HBDungeonModuleCatalog::Build({ Recovery }, { Corridor }, Catalog, Errors));
    if (Catalog.Num() != 2) { AddError(FString::Join(Errors, TEXT(";"))); return false; }
    TestEqual(TEXT("Role is Recovery"), Catalog[0].Role, EHBPrototypeRoomRole::Recovery);
    TestTrue(TEXT("Candidate setting comes from the room component"), Catalog[0].bIsStartCandidate);
    TestTrue(TEXT("Bounds use scaled occupancy"), Catalog[0].Bounds.Min.Equals(FVector(0,0,0)) && Catalog[0].Bounds.Max.Equals(FVector(400,600,200)));
    TestEqual(TEXT("Corridor has two connectors"), Catalog[1].ConnectorCount, 2);
    TestTrue(TEXT("Geometry preserved"), Catalog[0].Occupancy.Num() == 1 && Catalog[0].Connectors.Num() == 1);
    TestFalse(TEXT("Reject duplicate map"), HBDungeonModuleCatalog::Build({Recovery,Recovery}, {}, Catalog, Errors));
    TestTrue(TEXT("No partial catalog on error"), Catalog.IsEmpty());
    UWorld* Duplicate = Fixture.Add(TEXT("NotNamedHeal"), EHBPrototypeRoomRole::Boss);
    TestFalse(TEXT("Reject duplicate ModuleId across maps"), HBDungeonModuleCatalog::Build({Recovery,Duplicate}, {}, Catalog, Errors));
    TestFalse(TEXT("Reject wrong module kind"), HBDungeonModuleCatalog::Build({Corridor}, {}, Catalog, Errors));
    TestFalse(TEXT("Reject empty array slot"), HBDungeonModuleCatalog::Build({TSoftObjectPtr<UWorld>()}, {}, Catalog, Errors));
    UWorld* Unconfigured = Fixture.Add(TEXT("Unconfigured"), EHBPrototypeRoomRole::Unassigned);
    TestFalse(TEXT("Reject missing role instead of guessing"), HBDungeonModuleCatalog::Build({Unconfigured}, {}, Catalog, Errors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBCatalogSelectionTest, "HeavenBreach.PCGModule.CatalogSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBCatalogSelectionTest::RunTest(const FString&)
{
    FCatalogFixture Fixture;
    TArray<TSoftObjectPtr<UWorld>> Maps;
    for (int32 Role = 0; Role < 4; ++Role)
        for (int32 Variant = 0; Variant < 2; ++Variant)
            Maps.Add(Fixture.Add(FName(*FString::Printf(TEXT("Module_%d_%d"), Role, Variant)), static_cast<EHBPrototypeRoomRole>(Role)));
    TArray<FHBModuleCatalogEntry> Catalog;
    TArray<FString> Errors;
    if (!TestTrue(TEXT("Extract eight role-specific maps"), HBDungeonModuleCatalog::Build(Maps, {}, Catalog, Errors))) { return false; }
    TSet<FName> Selected;
    for (int32 Seed = 0; Seed < 32; ++Seed)
    {
        HBDungeonLayout::FSettings Settings; Settings.Seed = Seed;
        TArray<FHBPrototypeRoom> A, B; FString Error;
        if (!TestTrue(TEXT("Catalog layout generates"), HBDungeonModuleCatalog::Generate(Catalog, Settings, A, Error))) { AddError(Error); return false; }
        TestTrue(TEXT("Same seed replays"), HBDungeonModuleCatalog::Generate(Catalog, Settings, B, Error));
        TestEqual(TEXT("Replay count"), A.Num(), B.Num());
        if (A.Num() != B.Num()) { return false; }
        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            const auto& Room = A[Index];
            const auto* Entry = Catalog.FindByPredicate([&](const auto& E) { return E.ModuleId == Room.ModuleId; });
            if (!TestNotNull(TEXT("Selected module exists"), Entry)) { return false; }
            TestEqual(TEXT("Fixed role respected"), Room.Role, Entry->Role);
            TestTrue(TEXT("Loader map matches chosen module"), Room.ModuleMap == Entry->Map);
            TestEqual(TEXT("Same seed same module"), Room.ModuleId, B[Index].ModuleId);
            TestTrue(TEXT("Same seed same transform"), Room.Transform.Equals(B[Index].Transform));
            TestTrue(TEXT("Selected bounds used"), Room.Bounds.Equals(Entry->Bounds.TransformBy(Room.Transform), 0.01));
            Selected.Add(Room.ModuleId);
        }
    }
    TestEqual(TEXT("All variants can be selected"), Selected.Num(), 8);
    Catalog.RemoveAll([](const auto& E) { return E.Role == EHBPrototypeRoomRole::Recovery; });
    TArray<FHBPrototypeRoom> Rooms; FString Error;
    TestFalse(TEXT("Missing role fails instead of substituting Adventure"), HBDungeonModuleCatalog::Generate(Catalog, HBDungeonLayout::FSettings(), Rooms, Error));
    TestTrue(TEXT("Missing role diagnostic"), Error.Contains(TEXT("RECOVERY")));
    TestTrue(TEXT("No partial layout"), Rooms.IsEmpty());
    return true;
}
#endif
