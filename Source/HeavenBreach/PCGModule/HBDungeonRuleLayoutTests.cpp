#include "PCGModule/HBDungeonRuleLayout.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonMarkerActors.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "IpNetDriver.h"
#include "Engine/ChildConnection.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
namespace
{
	struct FLayoutLogCapture : FOutputDevice
	{
		TArray<FString> Messages;
		FLayoutLogCapture() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FLayoutLogCapture() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
		void Serialize(const TCHAR* Text, ELogVerbosity::Type, const FName& Category) override
		{
			if (Category == FName(TEXT("LogHBDungeonLayout"))) Messages.Add(Text);
		}
		void Reset() { GLog->FlushThreadedLogs(); Messages.Reset(); }
		FString Text() { GLog->FlushThreadedLogs(); return FString::Join(Messages, TEXT("\n")); }
	};
	FHBModuleCatalogEntry MakeModule(const TCHAR* Name, EHBPrototypeRoomRole Role, EHBAllowedFloor Floor = EHBAllowedFloor::Any)
	{
		FHBModuleCatalogEntry M;
		M.ModuleId = Name; M.Role = Role; M.AllowedFloor = Floor;
		M.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("/Game/Tests/%s.%s"), Name, Name)));
		M.Occupancy.Add({FTransform(FVector(0, 0, 100)), FVector(100)});
		M.Bounds = FBox(FVector(-100, -100, 0), FVector(100, 100, 200));
		for (int32 I = 0; I < 4; ++I)
		{
			const FRotator Rotation(0, I * 90, 0);
			M.Connectors.Add({FName(*FString::Printf(TEXT("Door%d"), I)), FTransform(Rotation, Rotation.Vector() * 100)});
		}
		M.ConnectorCount = M.Connectors.Num();
		return M;
	}
	struct FCatalog
	{
		TArray<FHBModuleCatalogEntry> Modules;
		FHBDungeonGenerationRules Rules;
		HBConnectorLayout::FSettings Settings;
		FCatalog()
		{
			Modules = {MakeModule(TEXT("Ordinary"), EHBPrototypeRoomRole::Adventure),
				MakeModule(TEXT("Rest"), EHBPrototypeRoomRole::Recovery), MakeModule(TEXT("Upgrade"), EHBPrototypeRoomRole::Upgrade),
				MakeModule(TEXT("UserChosenDeparture"), EHBPrototypeRoomRole::Adventure, EHBAllowedFloor::Floor1),
				MakeModule(TEXT("UserChosenArrival"), EHBPrototypeRoomRole::Adventure, EHBAllowedFloor::Floor2),
				MakeModule(TEXT("FinalRoom"), EHBPrototypeRoomRole::Boss, EHBAllowedFloor::Floor2),
				MakeModule(TEXT("UserChosenHall"), EHBPrototypeRoomRole::Unassigned, EHBAllowedFloor::Floor2)};
			Modules[6].Kind = EHBDungeonModuleKind::Corridor;
			Modules[0].bIsStartCandidate = true;
			for (auto& C : Modules[5].Connectors) C.Role = EHBDungeonConnectorRole::BossEntrance;
			Modules[6].Connectors[0].Role = EHBDungeonConnectorRole::DungeonSide;
			Modules[6].Connectors[2].Role = EHBDungeonConnectorRole::BossSide;
			Modules[3].Portals.Add({TEXT("Out"), FTransform::Identity, FVector(10), FTransform(FVector(30, 0, 0))});
			Modules[4].Portals.Add({TEXT("In"), FTransform::Identity, FVector(10), FTransform(FVector(30, 0, 0))});
			Rules.Floor1ExitMap = Modules[3].Map; Rules.Floor1ExitPortalId = TEXT("Out");
			Rules.Floor2EntryMap = Modules[4].Map; Rules.Floor2EntryPortalId = TEXT("In");
			Rules.BossCorridorMap = Modules[6].Map;
			Settings.MaxSearchSeconds = 10;
			Settings.MaxSearchSteps = 200000;
			Settings.Origin = FTransform(FRotator(0, 35, 0), FVector(2000, -900, 700));
		}
	};
	FString RuleLayoutSignature(const TArray<FHBPrototypeRoom>& Rooms)
	{
		FString S = HBDungeonLayout::Signature(Rooms);
		for (const auto& R : Rooms) S += R.ModuleId.ToString() + R.ParentConnectorId.ToString() + R.EntryConnectorId.ToString()
			+ R.ParentPortalId.ToString() + R.EntryPortalId.ToString() + (R.bIsStartRoom ? TEXT("Start") : TEXT("Ordinary"));
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConnectorRolesTest, "HeavenBreach.PCGModule.Rules.ConnectorRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConnectorRolesTest::RunTest(const FString&)
{
    using R = EHBDungeonConnectorRole;
    const bool Expected[4][4] = {{true,false,false,true},{false,false,true,false},{false,true,false,false},{true,false,false,false}};
    for (int32 A = 0; A < 4; ++A)
        for (int32 B = 0; B < 4; ++B)
            TestEqual(TEXT("Exact symmetric role allow-list"), HBConnectorRolesMatch(static_cast<R>(A), static_cast<R>(B)), Expected[A][B]);
    TestFalse(TEXT("Unknown roles rejected"), HBConnectorRolesMatch(static_cast<R>(255), R::None));
    FCatalog C;
    TArray<FHBPrototypeRoom> Rooms;
    FString Error;
    if (!TestTrue(TEXT("Directed boss corridor generates"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)))
    { AddError(Error); return false; }
    for (const auto& Room : Rooms)
    {
        if (Room.bCorridor) TestEqual(TEXT("Dungeon enters Dungeon Side"), Room.EntryConnectorId, FName(TEXT("Door0")));
        if (Room.Role == EHBPrototypeRoomRole::Boss) TestEqual(TEXT("Boss follows Boss Side"), Room.ParentConnectorId, FName(TEXT("Door2")));
    }
    // Existing geometry stays aligned: changing only the roles must invalidate the result.
    Swap(C.Modules[6].Connectors[0].Role, C.Modules[6].Connectors[2].Role);
    TestFalse(TEXT("Reversed role assignment rejects existing plan"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    TestFalse(TEXT("Physical validator also rejects incompatible roles"), HBConnectorLayout::Validate(C.Modules, Rooms, Error));
    C.Modules[6].Connectors[0].Role = R::None;
    TestFalse(TEXT("Missing Boss Side fails before search"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    TestTrue(TEXT("Missing role diagnostic"), Error.Contains(TEXT("Boss Side")));
    C = FCatalog();
    for (auto& Door : C.Modules[5].Connectors) Door.Role = R::None;
    TestFalse(TEXT("Boss needs explicit entrance"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    TestTrue(TEXT("Missing entrance diagnostic"), Error.Contains(TEXT("Boss Entrance")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConnectorNavigationEntryTest, "HeavenBreach.PCGModule.Connector.NavigationEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConnectorNavigationEntryTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
    auto* Definition = World->SpawnActor<AHBDungeonModuleDefinition>();
    Definition->ModuleId = TEXT("NavigationEntryRoom");
    Definition->RoomInfo->RoomRole = EHBPrototypeRoomRole::Adventure;
    World->SpawnActor<AHBDungeonOccupancy>();
    auto* Marker = World->SpawnActor<AHBDungeonConnector>();
    auto* Connector = Marker->FindComponentByClass<UHBDungeonConnectorComponent>();
    if (TestNotNull(TEXT("Connector marker exposes its authoring component"), Connector))
    {
        Connector->ConnectorId = TEXT("Entry");
        Marker->SetActorLocationAndRotation(FVector(1000, 2000, 300), FRotator(0, 90, 0));
        const FTransform Connection = Connector->GetComponentTransform();
        TestTrue(TEXT("Default zero offset uses the existing connection coordinate"),
            Connector->NavigationEntryOffset.IsZero() && Connector->GetNavigationEntryLocation().Equals(Connection.GetLocation()));
        const auto* Property = FindFProperty<FStructProperty>(Connector->GetClass(), TEXT("NavigationEntryOffset"));
        if (TestNotNull(TEXT("Navigation offset is a reflected authoring property"), Property))
            TestTrue(TEXT("Offset is editable in BP defaults and placed connector instances"),
                Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
                && !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate));
        const auto* Function = Connector->FindFunction(TEXT("GetNavigationEntryLocation"));
        TestTrue(TEXT("World-space navigation target is available as a pure Blueprint function"),
            Function && Function->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure));
        Connector->NavigationEntryOffset = FVector(-200, 0, 40);
        TestTrue(TEXT("Local inward offset rotates with the room and retains vertical adjustment"),
            Connector->GetNavigationEntryLocation().Equals(FVector(1000, 1800, 340), 0.01));
        TestTrue(TEXT("Offset leaves the physical connector transform untouched"), Connector->GetComponentTransform().Equals(Connection));
        TArray<FHBModuleCatalogEntry> Catalog;
        TArray<FString> Errors;
        if (TestTrue(TEXT("Module with an offset target remains valid for assembly"), HBDungeonModuleCatalog::Build({World}, Catalog, Errors))
            && TestEqual(TEXT("Catalog retains the authored room"), Catalog.Num(), 1)
            && TestEqual(TEXT("Catalog retains the authored connector"), Catalog[0].Connectors.Num(), 1))
            TestTrue(TEXT("Assembly catalog uses the physical marker, not the navigation target"),
                Catalog[0].Connectors[0].LocalTransform.Equals(Connection));
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalSelectionTest, "HeavenBreach.PCGModule.Rules.PortalSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalSelectionTest::RunTest(const FString&)
{
    FCatalog C;
    FString Error;
    C.Rules.Floor1ExitPortalId = NAME_None;
    C.Rules.Floor2EntryPortalId = NAME_None;
    auto R = C.Rules;
    TestTrue(TEXT("Unique portals resolve automatically"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, R, Error));
    TestEqual(TEXT("Exit resolved"), R.Floor1ExitPortalId, FName(TEXT("Out")));
    TestEqual(TEXT("Entry resolved"), R.Floor2EntryPortalId, FName(TEXT("In")));
    TArray<FHBPrototypeRoom> Rooms;
    TestTrue(TEXT("Generation accepts automatic IDs"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    TestTrue(TEXT("Validation accepts automatic IDs"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    R.Floor1ExitPortalId = TEXT("Deleted");
    TestFalse(TEXT("Deleted selection is never substituted"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, R, Error));
    TestEqual(TEXT("Invalid selection preserved"), R.Floor1ExitPortalId, FName(TEXT("Deleted")));
    C.Modules[3].Portals.Add({TEXT("Other"), FTransform::Identity, FVector(10), FTransform::Identity});
    R = C.Rules;
    TestFalse(TEXT("Multiple portals require selection"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, R, Error));
    R.Floor1ExitPortalId = TEXT("Other");
    TestTrue(TEXT("Explicit choice resolves ambiguity"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, R, Error));
    C.Modules[4].Portals.Reset(); R = C.Rules; R.Floor1ExitPortalId = TEXT("Out");
    TestFalse(TEXT("Absent portal rejected"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, R, Error));
    C.Rules.Floor1ExitMap.Reset();
    TestFalse(TEXT("Unregistered map rejected"), HBDungeonRuleLayout::ResolvePortalIds(C.Modules, C.Rules, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRuleGenerationTest, "HeavenBreach.PCGModule.Rules.Generation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBRuleGenerationTest::RunTest(const FString&)
{
	FCatalog C;
	TSet<FString> Results;
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		C.Settings.Seed = Seed;
		TArray<FHBPrototypeRoom> A, B; FString Error;
		if (!TestTrue(TEXT("Rule layout generated"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, A, Error))) { AddError(Error); return false; }
		TestEqual(TEXT("11 rooms plus one corridor"), A.Num(), 12);
		TestTrue(TEXT("Independent full validation"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, A, Error));
		TestTrue(TEXT("Seed replay"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, B, Error));
		TestEqual(TEXT("Stable replay including connectors and portal IDs"), RuleLayoutSignature(A), RuleLayoutSignature(B));
		Results.Add(RuleLayoutSignature(A));
		int32 Bosses = 0, Corridors = 0, PortalEdges = 0, UsedDoors = 0;
		for (const auto& R : A)
		{
			UsedDoors += R.UsedConnectorIds.Num();
			if (R.bCorridor) { ++Corridors; TestEqual(TEXT("Selected corridor, independent of name"), R.ModuleMap, C.Rules.BossCorridorMap); }
			if (R.Role == EHBPrototypeRoomRole::Boss)
			{
				++Bosses; TestEqual(TEXT("Boss floor"), R.Floor, 2);
				TestTrue(TEXT("Boss directly follows corridor"), A[R.ParentId].bCorridor);
			}
			if (R.Connection == EHBDungeonConnection::Portal)
			{
				++PortalEdges;
				TestEqual(TEXT("User chosen entry"), R.ModuleMap, C.Rules.Floor2EntryMap);
				TestEqual(TEXT("User chosen exit"), A[R.ParentId].ModuleMap, C.Rules.Floor1ExitMap);
				TestTrue(TEXT("Logical second floor can use the same world Z"), FMath::IsNearlyEqual(R.Transform.GetLocation().Z, A[0].Transform.GetLocation().Z));
			}
		}
		TestEqual(TEXT("One boss"), Bosses, 1); TestEqual(TEXT("One corridor"), Corridors, 1); TestEqual(TEXT("One portal edge"), PortalEdges, 1);
		TestEqual(TEXT("Portal edge consumes no door connectors"), UsedDoors, 2 * (A.Num() - 2));
		B = A; B[4].EntryPortalId = TEXT("Absent");
		TestFalse(TEXT("Missing portal rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, B, Error));
		B = A; B[4].Floor = 1;
		TestFalse(TEXT("Wrong floor rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, B, Error));
		B = A; B[7].bCorridor = false;
		TestFalse(TEXT("Corridor kind tampering rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, B, Error));
		B = A; B[8].bBranch = true;
		TestFalse(TEXT("Boss cannot be optional"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, B, Error));
		B = A; B[3].bFloorExit = false;
		TestFalse(TEXT("Missing exit marker rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, B, Error));
	}
	TestTrue(TEXT("Different seeds produce multiple layouts"), Results.Num() > 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBAdventureFloorResetTest, "HeavenBreach.PCGModule.Rules.AdventureFloorReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBAdventureFloorResetTest::RunTest(const FString&)
{
    FCatalog C;
    C.Modules.RemoveAll([](const auto& M) { return M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade; });
    C.Rules.Floor1MainRooms = 3; C.Rules.Floor2MainRooms = 4;
    C.Rules.Floor1BranchRooms = 0; C.Rules.Floor2BranchRooms = 1;
    TArray<FHBPrototypeRoom> Rooms; FString Error;
    if (!TestTrue(TEXT("Three adventures on each side of portal accepted"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Independent validation resets streak at floor boundary"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    for (int32 I = 0; I < 6; ++I)
        TestEqual(TEXT("Three plus three consecutive adventures across portal"), Rooms[I].Role, EHBPrototypeRoomRole::Adventure);
    TestEqual(TEXT("Arrival starts second floor"), Rooms[3].Floor, 2);
    C.Rules.Floor1MainRooms = 4;
    TestFalse(TEXT("Four adventures on floor 1 still rejected"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    C.Rules.Floor1MainRooms = 3; C.Rules.Floor2MainRooms = 5;
    TestFalse(TEXT("Four adventures on floor 2 still rejected"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
    C.Rules.Floor1MainRooms = 2; C.Rules.Floor2MainRooms = 3;
    C.Rules.Floor1BranchRooms = 1; C.Rules.Floor2BranchRooms = 2;
    TestTrue(TEXT("Reported 2/3 main and 1/2 branch configuration now allowed"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));

    FCatalog V;
    if (!TestTrue(TEXT("Generate valid layout with a special room breaking floor-1 streak"), HBDungeonRuleLayout::Generate(V.Modules, V.Settings, V.Rules, Rooms, Error)))
    { AddError(Error); return false; }
    for (auto& M : V.Modules)
        if (M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade) M.Role = EHBPrototypeRoomRole::Adventure;
    for (auto& R : Rooms)
        if (R.Role == EHBPrototypeRoomRole::Recovery || R.Role == EHBPrototypeRoomRole::Upgrade) R.Role = EHBPrototypeRoomRole::Adventure;
    TestFalse(TEXT("Independent validator rejects same-floor streak after role tampering"), HBDungeonRuleLayout::Validate(V.Modules, V.Settings, V.Rules, Rooms, Error));
    TestTrue(TEXT("Validator identifies same-floor streak"), Error.Contains(TEXT("consecutive Adventure rooms on the same floor")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConsecutiveRoleLimitsTest, "HeavenBreach.PCGModule.Rules.ConsecutiveRoleLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConsecutiveRoleLimitsTest::RunTest(const FString&)
{
	FCatalog Adventure;
	Adventure.Modules.RemoveAll([](const auto& M) { return M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade; });
	Adventure.Rules.Floor1MainRooms = 4; Adventure.Rules.Floor2MainRooms = 5;
	Adventure.Rules.Floor1BranchRooms = 0; Adventure.Rules.Floor2BranchRooms = 1;
	TArray<FHBPrototypeRoom> Rooms, Replay; FString Error;
	TestFalse(TEXT("Default Adventure limit rejects four on either floor"), HBDungeonRuleLayout::Generate(Adventure.Modules, Adventure.Settings, Adventure.Rules, Rooms, Error));
	Adventure.Rules.MaxConsecutiveAdventureRooms = 4;
	if (!TestTrue(TEXT("Adventure limit four permits the selected layout"), HBDungeonRuleLayout::Generate(Adventure.Modules, Adventure.Settings, Adventure.Rules, Rooms, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Validator uses Adventure limit four"), HBDungeonRuleLayout::Validate(Adventure.Modules, Adventure.Settings, Adventure.Rules, Rooms, Error));
	auto Stricter = Adventure.Rules; Stricter.MaxConsecutiveAdventureRooms = 3;
	TestFalse(TEXT("Same layout fails independent validation under Adventure limit three"), HBDungeonRuleLayout::Validate(Adventure.Modules, Adventure.Settings, Stricter, Rooms, Error));
	TestTrue(TEXT("Adventure diagnostic reports the configured limit"), Error.Contains(TEXT("More than 3 consecutive Adventure")));

	for (const auto Role : {EHBPrototypeRoomRole::Recovery, EHBPrototypeRoomRole::Upgrade})
	{
		FCatalog C;
		// Two Adventures are forced before a pair of special rooms ending floor 1.
		// The same special role starts floor 2, exercising both streak and quota reset.
		C.Rules.Floor1MainRooms = 4; C.Rules.Floor2MainRooms = 3;
		C.Rules.Floor1BranchRooms = 0; C.Rules.Floor2BranchRooms = 1;
		C.Rules.MaxConsecutiveAdventureRooms = 2;
		C.Modules[3].Role = C.Modules[4].Role = Role;
		for (auto& M : C.Modules)
			if (M.Role == Role && M.Map != C.Rules.Floor2EntryMap) M.AllowedFloor = EHBAllowedFloor::Floor1;
		C.Modules.RemoveAll([Role](const auto& M)
		{
			return (M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade) && M.Role != Role;
		});
		int32& Limit = Role == EHBPrototypeRoomRole::Recovery ? C.Rules.MaxConsecutiveRecoveryRooms : C.Rules.MaxConsecutiveEnhancementRooms;
		int32& Quota = Role == EHBPrototypeRoomRole::Recovery ? C.Rules.MaxRecoveryRoomsPerFloor : C.Rules.MaxEnhancementRoomsPerFloor;
		Quota = 2;
		FLayoutLogCapture Log;
		TestFalse(TEXT("Special limit one rejects the required adjacent pair despite quota two"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
		TestTrue(TEXT("Candidate filter reports the correct role streak"), Log.Text().Contains(Role == EHBPrototypeRoomRole::Recovery ? TEXT("RecoveryStreakLimit") : TEXT("EnhancementStreakLimit")));
		Limit = 2;
		if (!TestTrue(TEXT("Special limit two and quota two allow the adjacent pair"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)))
		{ AddError(Error); return false; }
		TestEqual(TEXT("Special pair begins on floor 1"), Rooms[2].Role, Role);
		TestEqual(TEXT("Selected exit completes the special pair"), Rooms[3].Role, Role);
		TestEqual(TEXT("Same special role can start the next floor"), Rooms[4].Role, Role);
		TestTrue(TEXT("Independent validator resets special streak and quota at the floor boundary"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
		TestTrue(TEXT("Custom role limits replay with the same seed"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Replay, Error));
		TestEqual(TEXT("Custom role limit replay is deterministic"), RuleLayoutSignature(Rooms), RuleLayoutSignature(Replay));
		Limit = 1;
		TestFalse(TEXT("Validator rejects the adjacent pair under special limit one"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
		TestTrue(TEXT("Special streak diagnostic includes the configured limit"), Error.Contains(TEXT("More than 1 consecutive")));
		Limit = 2; Quota = 1;
		TestFalse(TEXT("Validator enforces the separate per-floor quota"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Rooms, Error));
		TestTrue(TEXT("Quota diagnostic includes the configured limit"), Error.Contains(TEXT("per-floor limit of 1")));
		TestFalse(TEXT("Candidate filter also enforces the separate per-floor quota"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Replay, Error));
	}

	FCatalog Disabled;
	Disabled.Rules.MaxConsecutiveAdventureRooms = 4;
	Disabled.Rules.MaxRecoveryRoomsPerFloor = Disabled.Rules.MaxEnhancementRoomsPerFloor = 0;
	if (!TestTrue(TEXT("Zero special quotas permit an Adventure-only ordinary layout"), HBDungeonRuleLayout::Generate(Disabled.Modules, Disabled.Settings, Disabled.Rules, Rooms, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Zero quotas exclude both special roles, including branches"), !Rooms.ContainsByPredicate([](const auto& R)
	{ return R.Role == EHBPrototypeRoomRole::Recovery || R.Role == EHBPrototypeRoomRole::Upgrade; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRuleRejectionTest, "HeavenBreach.PCGModule.Rules.InvalidSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBRuleRejectionTest::RunTest(const FString&)
{
	FCatalog C; TArray<FHBPrototypeRoom> Rooms; FString Error;
	auto Reject = [&](const TCHAR* What, const TArray<FHBModuleCatalogEntry>& Catalog, const FHBDungeonGenerationRules& Rules)
	{
		TestFalse(What, HBDungeonRuleLayout::Generate(Catalog, C.Settings, Rules, Rooms, Error));
		TestTrue(TEXT("Failure exposes no partial dungeon"), Rooms.IsEmpty());
		TestFalse(TEXT("Failure explains why"), Error.IsEmpty());
	};
	auto R = C.Rules; R.Floor1ExitMap.Reset(); Reject(TEXT("Unselected exit"), C.Modules, R);
	R = C.Rules; R.Floor2EntryPortalId = TEXT("Missing"); Reject(TEXT("Missing selected portal"), C.Modules, R);
	R = C.Rules; R.BossCorridorMap = C.Modules[0].Map; Reject(TEXT("A room cannot masquerade as a corridor"), C.Modules, R);
	R = C.Rules; R.Floor2BranchRooms = R.Floor1BranchRooms; Reject(TEXT("Floor 2 requires more branches"), C.Modules, R);
	R = C.Rules; R.MaxConsecutiveAdventureRooms = 0; Reject(TEXT("Adventure consecutive limit must be positive"), C.Modules, R);
	R = C.Rules; R.MaxConsecutiveRecoveryRooms = 0; Reject(TEXT("Recovery consecutive limit must be positive"), C.Modules, R);
	R = C.Rules; R.MaxConsecutiveEnhancementRooms = -1; Reject(TEXT("Enhancement consecutive limit must be positive"), C.Modules, R);
	R = C.Rules; R.MaxRecoveryRoomsPerFloor = -1; Reject(TEXT("Recovery per-floor limit cannot be negative"), C.Modules, R);
	R = C.Rules; R.MaxEnhancementRoomsPerFloor = -1; Reject(TEXT("Enhancement per-floor limit cannot be negative"), C.Modules, R);
	auto Modules = C.Modules; Modules[4].AllowedFloor = EHBAllowedFloor::Floor1; Reject(TEXT("Entry forbidden on floor 2"), Modules, C.Rules);
	Modules = C.Modules; Modules[3].Portals.Reset(); Reject(TEXT("User must place the source portal"), Modules, C.Rules);
	Modules = C.Modules; Modules[5].AllowedFloor = EHBAllowedFloor::Floor1; Reject(TEXT("No eligible boss"), Modules, C.Rules);
	C.Settings.MaxSearchSteps = 1; Reject(TEXT("Bounded search"), C.Modules, C.Rules);
	TestTrue(TEXT("Search budget diagnostic"), Error.Contains(TEXT("Search limit")));
	C.Settings.MaxSearchSteps = 200000; C.Settings.bAllowModuleReuse = false;
	Reject(TEXT("Insufficient distinct modules"), C.Modules, C.Rules);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBStartCandidateLayoutTest, "HeavenBreach.PCGModule.Rules.StartCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBStartCandidateLayoutTest::RunTest(const FString&)
{
	FCatalog C;
	C.Modules[1].bIsStartCandidate = C.Modules[2].bIsStartCandidate = true;
	TSet<EHBPrototypeRoomRole> StartRoles;
	bool bRepeatedCandidate = false, bSeveralCandidates = false;
	FString Error;
	TArray<FHBPrototypeRoom> Rooms, Replay;
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		C.Settings.Seed = Seed;
		if (!HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)) { AddError(Error); return false; }
		TestTrue(TEXT("Replay succeeds"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Replay, Error));
		TestEqual(TEXT("Start instance and layout replay"), RuleLayoutSignature(Rooms), RuleLayoutSignature(Replay));
		TestTrue(TEXT("First instance is the actual start"), Rooms[0].bIsStartRoom);
		StartRoles.Add(Rooms[0].Role);
		TSet<FName> CandidateIds;
		int32 StartCount = 0;
		for (const auto& Room : Rooms)
		{
			StartCount += Room.bIsStartRoom ? 1 : 0;
			const auto* Module = C.Modules.FindByPredicate([&](const auto& M) { return M.ModuleId == Room.ModuleId; });
			if (Module->bIsStartCandidate)
			{
				bRepeatedCandidate |= CandidateIds.Contains(Room.ModuleId);
				CandidateIds.Add(Room.ModuleId);
			}
		}
		bSeveralCandidates |= CandidateIds.Num() > 1;
		TestEqual(TEXT("Only one instance is the actual start"), StartCount, 1);
	}
	TestEqual(TEXT("Adventure, Recovery and Upgrade can all start"), StartRoles.Num(), 3);
	TestTrue(TEXT("Several candidate maps can occur in one dungeon"), bSeveralCandidates);
	TestTrue(TEXT("A candidate map can occur more than once"), bRepeatedCandidate);
	Replay = Rooms; Replay[1].bIsStartRoom = true;
	TestFalse(TEXT("Two actual starts rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Replay, Error));
	Replay = Rooms; Replay[0].bIsStartRoom = false;
	TestFalse(TEXT("Missing actual start rejected"), HBDungeonRuleLayout::Validate(C.Modules, C.Settings, C.Rules, Replay, Error));

	for (auto& M : C.Modules) M.bIsStartCandidate = false;
	TestFalse(TEXT("Missing candidates rejected"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	TestTrue(TEXT("Missing candidate diagnostic"), Error.Contains(TEXT("start candidate")));
	C.Modules[5].bIsStartCandidate = true;
	TestFalse(TEXT("Boss cannot be a candidate"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	C.Modules[5].bIsStartCandidate = false;
	C.Modules[4].bIsStartCandidate = true;
	TestFalse(TEXT("No first-floor candidate rejected"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));

	// A transition map may also author a start candidate. Its ordinary copies do not acquire transition roles.
	C.Modules[4].bIsStartCandidate = false;
	C.Modules[3].bIsStartCandidate = true;
	if (!HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)) { AddError(Error); return false; }
	TestEqual(TEXT("Transition map can be selected as the start"), Rooms[0].ModuleMap, C.Rules.Floor1ExitMap);
	TestFalse(TEXT("Start copy is not the floor-exit instance"), Rooms[0].bFloorExit);
	TestTrue(TEXT("The separate exit instance retains the configured map"), Rooms.ContainsByPredicate([&](const auto& R)
		{ return R.bFloorExit && R.ModuleMap == C.Rules.Floor1ExitMap && !R.bIsStartRoom; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRuleFailureDiagnosticsTest, "HeavenBreach.PCGModule.Rules.FailureDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBRuleFailureDiagnosticsTest::RunTest(const FString&)
{
	FCatalog C;
	FLayoutLogCapture Capture;
	TArray<FHBPrototypeRoom> Rooms, Replay;
	FString Error;
	if (!TestTrue(TEXT("Valid layout still generates"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Successful search emits no failure diagnostics"), Capture.Text().IsEmpty());
	TestTrue(TEXT("Successful search still replays"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Replay, Error));
	TestEqual(TEXT("Diagnostics preserve seeded replay"), RuleLayoutSignature(Rooms), RuleLayoutSignature(Replay));

	Capture.Reset(); C.Settings.MaxSearchSteps = 1;
	TestFalse(TEXT("Attempt limit fails"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	FString Log = Capture.Text();
	TestTrue(TEXT("Attempt limit distinguished"), Log.Contains(TEXT("Stop=AttemptLimit")) && Error.Contains(TEXT("AttemptLimit")));
	TestTrue(TEXT("Settings include the seed and reuse policy"), Log.Contains(TEXT("Seed=12345")) && Log.Contains(TEXT("Reuse=true")));
	TestTrue(TEXT("Next slot and its parent retained after backtracking"), Log.Contains(TEXT("[NextSlot] Room=1 Floor=1 Kind=Room ParentRoom=0"))
		&& Log.Contains(TEXT("[ParentSnapshot] Room=0 Module=Ordinary")) && Log.Contains(TEXT("AvailableDoors=4/4")));
	TestTrue(TEXT("Normal candidate filtering is separately labeled"), Log.Contains(TEXT("[CandidateFilters]"))
		&& Log.Contains(TEXT("normal filtering; not placement failures")));
	TestTrue(TEXT("Failed generation returns no partial rooms"), Rooms.IsEmpty());
	TestTrue(TEXT("Failure summary remains bounded"), Capture.Messages.Num() <= 48);

	Capture.Reset(); C = FCatalog(); C.Settings.MaxSearchSeconds = 1.e-12;
	TestFalse(TEXT("Time limit fails"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	Log = Capture.Text();
	TestTrue(TEXT("Time limit distinguished without a fixed wait"), Log.Contains(TEXT("Stop=TimeLimit")) && Error.Contains(TEXT("TimeLimit")));
	TestTrue(TEXT("No attempted placement falsely reported"), Log.Contains(TEXT("Attempts=0/200000")) && Log.Contains(TEXT("Best=0/12")));

	Capture.Reset(); C = FCatalog();
	C.Rules.Floor1MainRooms = 3; C.Rules.Floor2MainRooms = 4;
	C.Rules.Floor1BranchRooms = 0; C.Rules.Floor2BranchRooms = 1;
	C.Modules[0].Connectors.SetNum(1); C.Modules[0].ConnectorCount = 1;
	TestFalse(TEXT("One-door chain cannot extend"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	Log = Capture.Text();
	TestTrue(TEXT("Exhaustion distinguished from budget limits"), Log.Contains(TEXT("Stop=Exhausted")));
	TestTrue(TEXT("Deepest door shortage reported"), Log.Contains(TEXT("Scope=Deepest Rejection=NoAvailableParentDoors"))
		&& Log.Contains(TEXT("ParentRoom=1 ParentModule=Ordinary")) && Log.Contains(TEXT("UsedDoors=1/1")));
	TestTrue(TEXT("Door snapshot is retained"), Log.Contains(TEXT("[ParentSnapshot] Room=1 Module=Ordinary")) && Log.Contains(TEXT("AvailableDoors=0/1")));

	Capture.Reset(); C = FCatalog(); C.Settings.MaxSearchSteps = 1000;
	C.Modules[5].Occupancy[0].Extent = FVector(10000, 10000, 100);
	C.Modules[5].Bounds = FBox(FVector(-10000, -10000, 0), FVector(10000, 10000, 200));
	TestFalse(TEXT("Oversized boss occupancy prevents completion"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	Log = Capture.Text();
	TestTrue(TEXT("Deepest collision identifies the proposed module"), Log.Contains(TEXT("Scope=Deepest Rejection=OccupancyOverlap Depth=8 Floor=2 Candidate=FinalRoom")));
	TestTrue(TEXT("Collision includes existing room and both box details"), Log.Contains(TEXT("OverlapWithRoom=")) && Log.Contains(TEXT("BoxAIndex=0 CenterA="))
		&& Log.Contains(TEXT("RotationA=")) && Log.Contains(TEXT("HalfExtentA=")) && Log.Contains(TEXT("BoxBIndex=0 CenterB=")) && Log.Contains(TEXT("HalfExtentB=")));
	TestTrue(TEXT("Door role mismatch is counted separately"), Log.Contains(TEXT("ConnectorRoleMismatch=")));
	TestTrue(TEXT("Summary distinguishes a snapshot from aggregated contexts"), Log.Contains(TEXT("Snapshot=FirstContextAtBest"))
		&& Log.Contains(TEXT("Examples may belong to different branches")));
	TestTrue(TEXT("Many attempts produce a bounded log"), Capture.Messages.Num() <= 48);

	// Exhaust all available Adventure maps, so the sole remaining branch is Recovery.
	// Two Adventures tie two Recoveries: CheckRules must retain its original final error.
	Capture.Reset(); C = FCatalog(); C.Settings.bAllowModuleReuse = false;
	C.Rules.Floor1MainRooms = 2; C.Rules.Floor2MainRooms = 3;
	C.Rules.Floor1BranchRooms = 0; C.Rules.Floor2BranchRooms = 1;
	C.Modules[0].bIsStartCandidate = false; C.Modules[1].bIsStartCandidate = true;
	C.Modules[4].Role = EHBPrototypeRoomRole::Upgrade;
	C.Modules.Add(MakeModule(TEXT("BranchRest"), EHBPrototypeRoomRole::Recovery, EHBAllowedFloor::Floor2));
	for (int32 I : {0, 1, 3, 4, 5, 7})
	{
		const int32 Doors = (I == 0 || I == 4) ? 2 : 1;
		if (Doors == 2) C.Modules[I].Connectors[1] = C.Modules[I].Connectors[2];
		C.Modules[I].Connectors.SetNum(Doors); C.Modules[I].ConnectorCount = Doors;
	}
	C.Modules[6].Connectors[1] = C.Modules[6].Connectors[2];
	C.Modules[6].Connectors.SetNum(2); C.Modules[6].ConnectorCount = 2;
	TestFalse(TEXT("All completed layouts violate the role balance"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	Log = Capture.Text();
	TestTrue(TEXT("Final-rule rejection retains the original reason"), Log.Contains(TEXT("Scope=Deepest Rejection=FinalRuleValidation Depth=7 Reason=Adventure must be the most common role.")));
	TestTrue(TEXT("Completed but rejected layouts are distinguished"), Log.Contains(TEXT("Best=7/7")) && Log.Contains(TEXT("All slots filled; final rule validation rejected")));
	TestTrue(TEXT("Final-rule reason appears in the returned failure summary"), Error.Contains(TEXT("FinalRuleValidation=")));
	TestTrue(TEXT("Reuse rejection recorded when reuse is disabled"), Log.Contains(TEXT("ModuleReuseDisabled=")));

	Capture.Reset(); C = FCatalog(); C.Rules.Floor2BranchRooms = C.Rules.Floor1BranchRooms;
	TestFalse(TEXT("Invalid settings fail before searching"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	Log = Capture.Text();
	TestTrue(TEXT("Settings failure has its own phase and original error"), Log.Contains(TEXT("Phase=Settings")) && Log.Contains(Error));
	TestFalse(TEXT("Preflight failure emits no search report"), Log.Contains(TEXT("[Search]")));
	Capture.Reset(); C = FCatalog(); C.Rules.Floor2EntryPortalId = TEXT("Missing");
	TestFalse(TEXT("Invalid portal selection fails"), HBDungeonRuleLayout::Generate(C.Modules, C.Settings, C.Rules, Rooms, Error));
	TestTrue(TEXT("Portal failure has its own phase and original error"), Capture.Text().Contains(TEXT("Phase=PortalSelection")) && Capture.Text().Contains(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalActorTest, "HeavenBreach.PCGModule.Portal.PairAndTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalActorTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* A = World->SpawnActor<AHBDungeonPortal>();
	auto* B = World->SpawnActor<AHBDungeonPortal>();
	A->PortalId = TEXT("Exit"); B->PortalId = TEXT("Entry");
	B->SetActorLocationAndRotation(FVector(5000, 0, 0), FRotator(0, 90, 0));
	FString Error;
	TestTrue(TEXT("Default authored geometry valid after ID assignment"), A->ValidateDefinition(Error));
	TestFalse(TEXT("No self link"), A->ConnectTo(A));
	TestTrue(TEXT("Pair connects"), A->ConnectTo(B));
	TestTrue(TEXT("Reciprocal link"), A->TargetPortal == B && B->TargetPortal == A);
	auto* PC = World->SpawnActor<APlayerController>();
	auto* Pawn = World->SpawnActor<AHBPCGExplorerPawn>();
	PC->Possess(Pawn);
	Pawn->SetActorLocation(FVector::ZeroVector);
	TestTrue(TEXT("Collision-free preview pawn traverses manually placed portal"), A->TryTeleport(Pawn));
	TestTrue(TEXT("Arrival honors rotated destination transform"), Pawn->GetActorLocation().Equals(B->Arrival->GetComponentLocation(), 0.01));
	TestTrue(TEXT("Arrival facing"), Pawn->GetActorRotation().Equals(B->Arrival->GetComponentRotation(), 0.01));
	Pawn->SetActorLocation(B->GetActorLocation());
	TestFalse(TEXT("Destination cannot immediately send the pawn back"), B->TryTeleport(Pawn));
	auto* OtherPC = World->SpawnActor<APlayerController>();
	auto* OtherPawn = World->SpawnActor<AHBPCGExplorerPawn>();
	OtherPC->Possess(OtherPawn); OtherPawn->SetActorLocation(FVector::ZeroVector);
	TestTrue(TEXT("Re-entry guard is per player, not shared"), A->TryTeleport(OtherPawn));
	// Non-replicated actors still have local authority in client worlds.
	auto* ClientProbePC = World->SpawnActor<APlayerController>();
	auto* ClientProbePawn = World->SpawnActor<AHBPCGExplorerPawn>();
	ClientProbePC->Possess(ClientProbePawn);
	World->AddController(ClientProbePC);
	ClientProbePawn->SetActorLocation(FVector::ZeroVector);
	auto* Driver = NewObject<UIpNetDriver>(World);
	Driver->SetWorld(World);
	Driver->ServerConnection = NewObject<UChildConnection>(Driver);
	World->SetNetDriver(Driver);
	TestTrue(TEXT("Fixture portals retain local actor authority"), A->HasAuthority());
	TestFalse(TEXT("Client cannot replace a portal pair"), A->ConnectTo(B));
	TestFalse(TEXT("Client cannot execute portal travel"), A->TryTeleport(ClientProbePawn));
	A->Tick(0.f);
	TestTrue(TEXT("Client portal tick leaves player location unchanged"), ClientProbePawn->GetActorLocation().Equals(FVector::ZeroVector));
	World->SetNetDriver(nullptr);
	Driver->ServerConnection = nullptr;
	Driver->SetWorld(nullptr);
	auto* CharacterPC = World->SpawnActor<APlayerController>();
	auto* Character = World->SpawnActor<ACharacter>();
	CharacterPC->Possess(Character);
	Character->SetActorLocation(FVector(0, 0, 100));
	auto* Obstacle = World->SpawnActor<AActor>();
	auto* Wall = NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(2000)); Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent(); Obstacle->SetActorLocation(B->Arrival->GetComponentLocation());
	const FVector Before = Character->GetActorLocation();
	TestTrue(TEXT("Fixture obstacle blocks the authored arrival"), World->EncroachingBlockingGeometry(Character, B->Arrival->GetComponentLocation(), B->Arrival->GetComponentRotation()));
	TestFalse(TEXT("Blocked character arrival is rejected"), A->TryTeleport(Character));
	TestTrue(TEXT("Blocked arrival preserves player location"), Character->GetActorLocation().Equals(Before, 0.01));
	A->Disconnect();
	TestTrue(TEXT("Disconnect clears both endpoints"), !A->TargetPortal && !B->TargetPortal);
	B->Arrival->SetRelativeLocation(FVector::ZeroVector);
	TestFalse(TEXT("Arrival within activation box rejected"), B->ValidateDefinition(Error));
	TestFalse(TEXT("Invalid destination cannot be linked"), A->ConnectTo(B));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalAuthoringTest, "HeavenBreach.PCGModule.Portal.Authoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalAuthoringTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
	auto* Definition = World->SpawnActor<AHBDungeonModuleDefinition>();
	Definition->ModuleId = TEXT("RoomWithPortal"); Definition->RoomInfo->RoomRole = EHBPrototypeRoomRole::Adventure;
	World->SpawnActor<AHBDungeonConnector>(); World->SpawnActor<AHBDungeonOccupancy>();
	auto* Portal = World->SpawnActor<AHBDungeonPortal>();
	TArray<FHBDungeonModuleIssue> Issues;
	TestFalse(TEXT("Unnamed portal rejected by module validation"), AHBDungeonModuleDefinition::ValidateLevel(World->PersistentLevel, Issues));
	Portal->PortalId = TEXT("Exit");
	TestTrue(TEXT("Named portal accepted"), AHBDungeonModuleDefinition::ValidateLevel(World->PersistentLevel, Issues));
	FString LookupError;
	ULevel* OtherInstance = NewObject<ULevel>(World, TEXT("OtherRoomInstance"));
	OtherInstance->OwningWorld = World;
	FActorSpawnParameters Spawn; Spawn.OverrideLevel = OtherInstance;
	auto* OtherPortal = World->SpawnActor<AHBDungeonPortal>(FVector(10000, 0, 0), FRotator::ZeroRotator, Spawn);
	OtherPortal->PortalId = Portal->PortalId;
	TestTrue(TEXT("Same PortalId resolves only in its selected instance"), AHBDungeonPortal::FindInLevel(World->PersistentLevel, TEXT("Exit"), LookupError) == Portal);
	TestTrue(TEXT("Other instance resolves its own portal"), AHBDungeonPortal::FindInLevel(OtherInstance, TEXT("Exit"), LookupError) == OtherPortal);
	TestNull(TEXT("Missing endpoint is not substituted"), AHBDungeonPortal::FindInLevel(OtherInstance, TEXT("Missing"), LookupError));
	OtherPortal->Destroy();
	Definition->RoomInfo->AllowedFloor = EHBAllowedFloor::Floor2;
	TArray<FHBModuleCatalogEntry> Catalog; TArray<FString> Errors;
	TestTrue(TEXT("Catalog extracts manually placed portal and allowed floor"), HBDungeonModuleCatalog::Build({World}, Catalog, Errors));
	if (Catalog.Num() == 1)
	{
		TestEqual(TEXT("Authored floor retained"), Catalog[0].AllowedFloor, EHBAllowedFloor::Floor2);
		TestEqual(TEXT("Authored portal count"), Catalog[0].Portals.Num(), 1);
		if (Catalog[0].Portals.Num() == 1)
			TestTrue(TEXT("Authored arrival transform retained"), Catalog[0].Portals[0].ArrivalTransform.Equals(Portal->Arrival->GetComponentTransform()));
	}
	auto* Duplicate = World->SpawnActor<AHBDungeonPortal>(); Duplicate->PortalId = Portal->PortalId;
	TestFalse(TEXT("Duplicate portal ID rejected per source map"), AHBDungeonModuleDefinition::ValidateLevel(World->PersistentLevel, Issues));
	TestNull(TEXT("Duplicate endpoint is not chosen arbitrarily"), AHBDungeonPortal::FindInLevel(World->PersistentLevel, TEXT("Exit"), LookupError));
	Duplicate->Destroy();
	Definition->RoomInfo->RoomRole = EHBPrototypeRoomRole::Boss; Definition->RoomInfo->AllowedFloor = EHBAllowedFloor::Floor1;
	TestFalse(TEXT("Boss authoring cannot force floor 1"), AHBDungeonModuleDefinition::ValidateLevel(World->PersistentLevel, Issues));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
