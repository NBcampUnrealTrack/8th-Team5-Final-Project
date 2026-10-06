#include "PCGModule/HBConnectorLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	void Rebound(FHBModuleCatalogEntry& M)
	{
		M.Bounds = FBox(ForceInit);
		for (const auto& B : M.Occupancy) { M.Bounds += FBox(-B.Extent,B.Extent).TransformBy(B.LocalTransform); }
		M.ConnectorCount = M.Connectors.Num();
	}
	FHBModuleCatalogEntry Module(FName Id, int32 Doors = 4)
	{
		FHBModuleCatalogEntry M;
		M.ModuleId = Id;
		M.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/") + Id.ToString() + TEXT(".") + Id.ToString()));
		M.Role = EHBPrototypeRoomRole::Recovery; // No Adventure/Boss requirement in the LD test.
		M.Occupancy.Add({FTransform(FVector(0,0,100)), FVector(100)});
		for (int32 I = 0; I < Doors; ++I)
		{
			const FRotator Rotation(0, I*90, 0);
			M.Connectors.Add({ FName(*FString::Printf(TEXT("Door_%d"),I)), FTransform(Rotation, Rotation.Vector()*100) });
		}
		Rebound(M);
		return M;
	}
	FString Signature(const TArray<FHBPrototypeRoom>& Rooms)
	{
		FString S = HBDungeonLayout::Signature(Rooms);
		for (const auto& R : Rooms) { S += R.ModuleId.ToString() + R.ParentConnectorId.ToString() + R.EntryConnectorId.ToString(); }
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConnectorAlignmentTest, "HeavenBreach.PCGModule.DirectAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConnectorAlignmentTest::RunTest(const FString&)
{
	const FTransform ParentLocal(FRotator(0,75,0), FVector(1300,-600,450));
	const FTransform ParentRoom(FRotator(0,-135,0), FVector(900,4000,200));
	const FTransform ChildLocal(FRotator(0,-33,0), FVector(-450,320,85));
	const FTransform PW = ParentLocal * ParentRoom;
	const FTransform ChildRoom = HBConnectorLayout::Align(PW,ChildLocal);
	const FTransform CW = ChildLocal * ChildRoom;
	TestTrue(TEXT("Noncentered connector positions coincide"), PW.GetLocation().Equals(CW.GetLocation(),0.001));
	TestTrue(TEXT("Directions oppose"), FVector::DotProduct(PW.GetRotation().GetForwardVector(),CW.GetRotation().GetForwardVector()) < -0.99999);
	TestTrue(TEXT("Rooms remain upright"), ChildRoom.GetRotation().GetUpVector().Equals(FVector::UpVector,0.0001));

	auto Elevated = Module(TEXT("Elevated"),2);
	Elevated.Connectors[1].LocalTransform.SetLocation(FVector(0,100,300));
	Elevated.Occupancy[0].Extent.Z = 250;
	Elevated.Occupancy[0].LocalTransform.SetLocation(FVector(0,0,250)); Rebound(Elevated);
	HBConnectorLayout::FSettings Settings; Settings.RoomCount = 3;
	TArray<FHBPrototypeRoom> Rooms; FString Error;
	TestTrue(TEXT("Height is determined by elevated connectors"), HBConnectorLayout::Generate({Elevated},Settings,Rooms,Error));
	if (Rooms.Num() == 3)
	{
		for (int32 I = 1; I < Rooms.Num(); ++I)
		{
			const auto& R = Rooms[I];
			const auto* P = Elevated.Connectors.FindByPredicate([&](const auto& C) { return C.ConnectorId == R.ParentConnectorId; });
			const auto* C = Elevated.Connectors.FindByPredicate([&](const auto& Entry) { return Entry.ConnectorId == R.EntryConnectorId; });
			const double ExpectedZ = Rooms[R.ParentId].Transform.GetLocation().Z
				+ P->LocalTransform.GetLocation().Z - C->LocalTransform.GetLocation().Z;
			TestTrue(TEXT("Elevation follows the selected connector pair"), FMath::IsNearlyEqual(R.Transform.GetLocation().Z, ExpectedZ, 0.001));
		}
	}
	const FTransform RaisedRoom = HBConnectorLayout::Align(Elevated.Connectors[1].LocalTransform, Elevated.Connectors[0].LocalTransform);
	TestTrue(TEXT("High exit to low entry raises the room by 300 cm"), FMath::IsNearlyEqual(RaisedRoom.GetLocation().Z, 300.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConnectorOccupancyTest, "HeavenBreach.PCGModule.DirectOccupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConnectorOccupancyTest::RunTest(const FString&)
{
	auto A = Module(TEXT("A")); auto B = Module(TEXT("B"));
	TestFalse(TEXT("Touching faces allowed"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,FTransform(FVector(200,0,0))));
	TestTrue(TEXT("Real penetration rejected including connected pairs"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,FTransform(FVector(199,0,0))));
	A.Occupancy[0].Extent = FVector(200,10,50);
	A.Occupancy[0].LocalTransform = FTransform(FRotator(0,45,0)); Rebound(A);
	B.Occupancy = A.Occupancy; Rebound(B);
	const FTransform Offset(FVector(-30/FMath::Sqrt(2.0),30/FMath::Sqrt(2.0),0));
	TestTrue(TEXT("Broad AABBs intersect"), A.Bounds.Intersect(B.Bounds.TransformBy(Offset)));
	TestFalse(TEXT("Separated rotated thin boxes pass narrow phase"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,Offset));
	TestTrue(TEXT("Same rotated boxes overlap"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,FTransform::Identity));
	A.Occupancy = {{FTransform(FVector(-200,0,0)),FVector(50)}, {FTransform(FVector(200,0,0)),FVector(50)}}; Rebound(A);
	B.Occupancy = {{FTransform::Identity,FVector(40)}}; Rebound(B);
	TestFalse(TEXT("Empty space between multiple boxes remains usable"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,FTransform::Identity));
	TestTrue(TEXT("Either occupancy box blocks placement"), HBConnectorLayout::OccupancyOverlaps(A,FTransform::Identity,B,FTransform(FVector(200,0,0))));
	HBConnectorLayout::FOccupancyOverlap Overlap;
	const FTransform RoomA(FRotator(0, 35, 0), FVector(1200, -700, 300));
	const FTransform RoomB = FTransform(FRotator(0, 20, 0), FVector(200, 0, 0)) * RoomA;
	TestTrue(TEXT("Collision details do not change rotated collision result"), HBConnectorLayout::OccupancyOverlaps(A, RoomA, B, RoomB, &Overlap));
	TestEqual(TEXT("First overlapping A box index"), Overlap.BoxAIndex, 1);
	TestEqual(TEXT("First overlapping B box index"), Overlap.BoxBIndex, 0);
	TestTrue(TEXT("A box world transform is retained"), Overlap.BoxAWorld.Equals(A.Occupancy[1].LocalTransform * RoomA, 0.001));
	TestTrue(TEXT("B box world transform is retained"), Overlap.BoxBWorld.Equals(B.Occupancy[0].LocalTransform * RoomB, 0.001));
	TestTrue(TEXT("Reported extents are box half-sizes"), Overlap.ExtentA.Equals(FVector(50)) && Overlap.ExtentB.Equals(FVector(40)));
	TestFalse(TEXT("Non-collision with optional details remains non-collision"), HBConnectorLayout::OccupancyOverlaps(A, RoomA, B, RoomA, &Overlap));
	TestEqual(TEXT("No-hit resets A index rather than retaining previous collision"), Overlap.BoxAIndex, INDEX_NONE);
	TestEqual(TEXT("No-hit resets B index"), Overlap.BoxBIndex, INDEX_NONE);
	TestTrue(TEXT("No-hit resets box details"), Overlap.BoxAWorld.Equals(FTransform::Identity) && Overlap.ExtentA.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBConnectorSearchTest, "HeavenBreach.PCGModule.DirectSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBConnectorSearchTest::RunTest(const FString&)
{
	TArray<FHBModuleCatalogEntry> Catalog = {Module(TEXT("Hub")),Module(TEXT("End"),1),Module(TEXT("Corner"),2)};
	Catalog[0].Role = EHBPrototypeRoomRole::Adventure;
	HBConnectorLayout::FSettings Settings; Settings.RoomCount = 8;
	Settings.Origin = FTransform(FRotator(0,25,0),FVector(1200,-800,300));
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		Settings.Seed = Seed;
		TArray<FHBPrototypeRoom> A,B; FString Error;
		if (!TestTrue(TEXT("Mixed door capacities produce exact count"),HBConnectorLayout::Generate(Catalog,Settings,A,Error))) { AddError(Error); return false; }
		TestEqual(TEXT("Requested room count"),A.Num(),8);
		TestTrue(TEXT("Independent physical validation"),HBConnectorLayout::Validate(Catalog,A,Error));
		TestTrue(TEXT("Replay generates"),HBConnectorLayout::Generate(Catalog,Settings,B,Error));
		TestEqual(TEXT("Same seed includes connector pairs and modules"),Signature(A),Signature(B));
		int32 Used = 0;
		for (const auto& Room : A) { Used += Room.UsedConnectorIds.Num(); }
		TestEqual(TEXT("Every edge consumes exactly two connectors"),Used,2*(A.Num()-1));
		B = A; B[1].Transform.AddToTranslation(FVector(5,0,0));
		const auto* M = Catalog.FindByPredicate([&](const auto& Entry){return Entry.ModuleId == B[1].ModuleId;});
		B[1].Bounds = M->Bounds.TransformBy(B[1].Transform);
		TestFalse(TEXT("Broken snap rejected"),HBConnectorLayout::Validate(Catalog,B,Error));
		B = A; B[0].UsedConnectorIds.Reset();
		TestFalse(TEXT("Incomplete port-use report rejected"),HBConnectorLayout::Validate(Catalog,B,Error));
	}
	TArray<FHBPrototypeRoom> Rooms; FString Error;
	Settings.RoomCount = 2;
	TestTrue(TEXT("One-door modules can form terminal pair"),HBConnectorLayout::Generate({Module(TEXT("End"),1)},Settings,Rooms,Error));
	Settings.RoomCount = 3;
	TestFalse(TEXT("Cannot reuse the only doorway for a third room"),HBConnectorLayout::Generate({Module(TEXT("End"),1)},Settings,Rooms,Error));
	TestTrue(TEXT("No partial layout on impossible fit"),Rooms.IsEmpty());
	Settings.bAllowModuleReuse = false; Settings.RoomCount = 3;
	TestTrue(TEXT("Distinct module mode can backtrack to fit"),HBConnectorLayout::Generate(Catalog,Settings,Rooms,Error));
	TSet<FName> Unique; for (const auto& R : Rooms) { Unique.Add(R.ModuleId); }
	TestEqual(TEXT("No repeated module"),Unique.Num(),3);
	Settings.RoomCount = 4;
	TestFalse(TEXT("Too few unique modules fails"),HBConnectorLayout::Generate(Catalog,Settings,Rooms,Error));
	Settings.bAllowModuleReuse = true; Settings.MaxSearchSteps = 1;
	TestFalse(TEXT("Search budget is bounded"),HBConnectorLayout::Generate(Catalog,Settings,Rooms,Error));
	TestTrue(TEXT("Budget diagnostic"),Error.Contains(TEXT("Search limit")));
	Settings.MaxSearchSteps = 20000;
	Catalog[0].Connectors[0].LocalTransform.SetRotation(FRotator(30,0,0).Quaternion());
	TestFalse(TEXT("Tilted doorway does not tilt the whole dungeon"),HBConnectorLayout::Generate(Catalog,Settings,Rooms,Error));
	TestTrue(TEXT("Tilt diagnostic"),Error.Contains(TEXT("horizontally")));
	return true;
}
#endif
