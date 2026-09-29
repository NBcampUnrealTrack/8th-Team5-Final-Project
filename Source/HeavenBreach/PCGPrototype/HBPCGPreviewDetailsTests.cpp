#include "PCGPrototype/HBPCGPreview.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "PropertyPath.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPreviewDetailsTest, "HeavenBreach.PCGModule.Preview.DetailsAndMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPreviewDetailsTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
    auto* Generator = World->SpawnActor<AHBPCGPreviewGenerator>();
    TestTrue(TEXT("New actors do not import obsolete settings"), Generator->bModuleRegistrationMigrated);
    const TSoftObjectPtr<UWorld> Room(FSoftObjectPath(TEXT("/Game/Tests/Room.Room")));
    const TSoftObjectPtr<UWorld> Hall(FSoftObjectPath(TEXT("/Game/Tests/Hall.Hall")));
    Generator->bModuleRegistrationMigrated = false;
    Generator->RoomModules = {Room}; Generator->CorridorModules = {Hall};
    Generator->DungeonRules.Floor1ExitMap = Room;
    Generator->DungeonRules.BossCorridorMap = Hall;
    Generator->PostLoad();
    TestEqual(TEXT("Legacy lists merged without duplicate registrations"), Generator->ModuleMaps.Num(), 2);
    TestTrue(TEXT("Legacy references cleared"), Generator->RoomModules.IsEmpty() && Generator->CorridorModules.IsEmpty());
    Generator->ModuleMaps.Reset(); Generator->PostLoad();
    TestTrue(TEXT("Removed registrations stay removed across reload"), Generator->ModuleMaps.IsEmpty());
    Generator->DungeonRules = {};
    auto& Editor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs Args; Args.bUpdatesFromSelection = false; Args.bLockable = false;
    auto View = Editor.CreateDetailView(Args);
    View->SetObject(Generator);
    auto HasProperty = [&](FName Name)
    {
        for (const auto& Path : View->GetPropertiesInOrderDisplayed())
            if (Path.IsValid() && Path.GetLeafMostProperty().Property.IsValid()
                && Path.GetLeafMostProperty().Property->GetFName() == Name) return true;
        return false;
    };
    auto CheckUniqueRows = [&]()
    {
        TSet<FString> Paths;
        for (const auto& Path : View->GetPropertiesInOrderDisplayed())
        {
            const FString Key = Path.ToString();
            TestFalse(*FString::Printf(TEXT("Property appears only once: %s"), *Key), Paths.Contains(Key));
            Paths.Add(Key);
        }
    };
    CheckUniqueRows();
    TestTrue(TEXT("Exploration controls present"), HasProperty(TEXT("MouseSensitivity")) && HasProperty(TEXT("bStartInExploration")));
    TestFalse(TEXT("Internal component references are not duplicated in actor properties"), HasProperty(TEXT("PCG")) || HasProperty(TEXT("PreviewCamera")));
    for (TFieldIterator<FProperty> It(AHBPCGPreviewGenerator::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FString Category = It->GetMetaData(TEXT("Category"));
        TestFalse(TEXT("No nested preview category mixed with custom categories"), Category.StartsWith(TEXT("PCG Preview|")));
        if (Category == TEXT("PCG Preview - Exploration"))
            TestTrue(TEXT("Exploration contains only user controls"), It->GetFName() == TEXT("MouseSensitivity") || It->GetFName() == TEXT("bStartInExploration"));
    }
    TestTrue(TEXT("Unified module registration visible"), HasProperty(TEXT("ModuleMaps")));
    TestFalse(TEXT("Legacy registration hidden"), HasProperty(TEXT("RoomModules")) || HasProperty(TEXT("CorridorModules")));
    TestFalse(TEXT("Raw portal IDs replaced with pickers"), HasProperty(TEXT("Floor1ExitPortalId")));
    TestTrue(TEXT("Rule counts visible in rule mode"), HasProperty(TEXT("Floor1MainRooms")));
    TestFalse(TEXT("Test count hidden in rule mode"), HasProperty(TEXT("TestRoomCount")));
    Generator->bUseDungeonRules = false; View->ForceRefresh();
    CheckUniqueRows();
    TestTrue(TEXT("Test count visible in test mode"), HasProperty(TEXT("TestRoomCount")));
    TestFalse(TEXT("Rule counts hidden in test mode"), HasProperty(TEXT("Floor1MainRooms")));
    auto Rows = Editor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
    for (bool bRules : {true, false})
    {
        Generator->bUseDungeonRules = bRules;
        Rows->SetObjects({Generator});
        TMap<FName, int32> CategoryCounts;
        TFunction<void(const TSharedRef<IDetailTreeNode>&)> Visit = [&](const auto& Node)
        {
            if (Node->GetNodeType() == EDetailNodeType::Category)
                ++CategoryCounts.FindOrAdd(Node->GetNodeName());
            TArray<TSharedRef<IDetailTreeNode>> Children;
            Node->GetChildren(Children);
            for (const auto& Child : Children) Visit(Child);
        };
        for (const auto& Root : Rows->GetRootTreeNodes()) Visit(Root);
        for (const auto& Pair : CategoryCounts)
            if (Pair.Key.ToString().StartsWith(TEXT("PCG Preview")))
                TestEqual(*FString::Printf(TEXT("Single category: %s"), *Pair.Key.ToString()), Pair.Value, 1);
        TestEqual(TEXT("Exactly one exploration category"), CategoryCounts.FindRef(TEXT("PCG Preview - Exploration")), 1);
        TestFalse(TEXT("No leftover preview parent section"), CategoryCounts.Contains(TEXT("PCG Preview")));
    }
    Rows->SetObjects({});
    View->SetObject(nullptr);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
