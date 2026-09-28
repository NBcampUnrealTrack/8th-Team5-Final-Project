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

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
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
			+ R.ParentPortalId.ToString() + R.EntryPortalId.ToString();
		return S;
	}
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
	auto Modules = C.Modules; Modules[4].AllowedFloor = EHBAllowedFloor::Floor1; Reject(TEXT("Entry forbidden on floor 2"), Modules, C.Rules);
	Modules = C.Modules; Modules[3].Portals.Reset(); Reject(TEXT("User must place the source portal"), Modules, C.Rules);
	Modules = C.Modules; Modules[5].AllowedFloor = EHBAllowedFloor::Floor1; Reject(TEXT("No eligible boss"), Modules, C.Rules);
	C.Settings.MaxSearchSteps = 1; Reject(TEXT("Bounded search"), C.Modules, C.Rules);
	TestTrue(TEXT("Search budget diagnostic"), Error.Contains(TEXT("Search limit")));
	C.Settings.MaxSearchSteps = 200000; C.Settings.bAllowModuleReuse = false;
	Reject(TEXT("Insufficient distinct modules"), C.Modules, C.Rules);
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
