#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "Loot/BoxActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace HBChestSpawnGeometry
{
	bool MakeGroundedTransform(const UClass* ChestClass, const FTransform& PointTransform,
		float FloorZ, FTransform& OutTransform, FString& OutError);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBChestGroundedTransformTest,
	"HeavenBreach.PCGModule.ChestSpawn.GroundedTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBChestGroundedTransformTest::RunTest(const FString&)
{
	constexpr float FloorZ = 1000.f;
	const FTransform PointTransform(FRotator(0, 45, 0), FVector(120, 240, FloorZ + 10.f));
	for (const TCHAR* Path : {
		TEXT("/Game/Loot/Blueprints/BP_Box_Common.BP_Box_Common_C"),
		TEXT("/Game/Loot/Blueprints/BP_Box_Celestial.BP_Box_Celestial_C") })
	{
		UClass* ChestClass = LoadClass<ABoxActor>(nullptr, Path);
		TestNotNull(TEXT("Existing fixed-grade box Blueprint loads"), ChestClass);
		if (!ChestClass) continue;
		const ABoxActor* Defaults = ChestClass->GetDefaultObject<ABoxActor>();
		const UStaticMeshComponent* Mesh = Defaults
			? Cast<UStaticMeshComponent>(Defaults->GetRootComponent()) : nullptr;
		TestTrue(TEXT("Box Blueprint has a root mesh"), Mesh && Mesh->GetStaticMesh());
		if (!Mesh || !Mesh->GetStaticMesh()) continue;
		FTransform Grounded;
		FString Error;
		TestTrue(TEXT("Box can be aligned to the traced floor"),
			HBChestSpawnGeometry::MakeGroundedTransform(ChestClass, PointTransform, FloorZ, Grounded, Error));
		const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetRelativeTransform() * Grounded);
		TestTrue(TEXT("Box mesh bottom clears the floor by 2 cm"),
			FMath::IsNearlyEqual(Bounds.Origin.Z - Bounds.BoxExtent.Z, FloorZ + 2.f, 0.1f));
		TestTrue(TEXT("Point horizontal position is preserved"),
			FVector::Dist2D(Grounded.GetLocation(), PointTransform.GetLocation()) < 0.01f);
		TestTrue(TEXT("Point rotation is preserved"), Grounded.GetRotation().Equals(PointTransform.GetRotation()));
		FTransform ElevatedPoint = PointTransform;
		ElevatedPoint.AddToTranslation(FVector(0, 0, 80));
		FTransform GroundedAgain;
		TestTrue(TEXT("Marker height does not raise the box off the floor"),
			HBChestSpawnGeometry::MakeGroundedTransform(ChestClass, ElevatedPoint, FloorZ, GroundedAgain, Error));
		TestTrue(TEXT("Both marker heights produce the same box height"),
			FMath::IsNearlyEqual(GroundedAgain.GetLocation().Z, Grounded.GetLocation().Z, 0.1f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBChestSpawnSelectionTest,
	"HeavenBreach.PCGModule.ChestSpawn.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBChestSpawnSelectionTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<AHBChestSpawnPoint*> Points;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		AHBChestSpawnPoint* Point = World->SpawnActor<AHBChestSpawnPoint>();
		Point->PointId = FName(*FString::Printf(TEXT("Chest_%02d"), Index));
		Points.Add(Point);
	}
	TestFalse(TEXT("Point actors alone do not define a room placement"),
		UHBChestSpawnSubsystem::HasPlacement(World->PersistentLevel));
	World->SpawnActor<AHBChestSetupActor>();
	TestTrue(TEXT("A setup actor is found in its own level"),
		UHBChestSpawnSubsystem::HasPlacement(World->PersistentLevel));

	UClass* CommonClass = ABoxActor::StaticClass();
	UClass* RareClass = LoadClass<ABoxActor>(nullptr,
		TEXT("/Game/Loot/Blueprints/BP_Box_Rare.BP_Box_Rare_C"));
	TestNotNull(TEXT("The existing rare box Blueprint is available"), RareClass);
	if (RareClass)
	{
		UHBChestPlacementComponent* Config = NewObject<UHBChestPlacementComponent>(GetTransientPackage());
		FHBChestPlacementGroup Group;
		Group.GroupId = TEXT("Loot");
		Group.MinCount = Group.MaxCount = 3;
		Group.Chests.Add({CommonClass, 1});
		Group.Chests.Add({RareClass, 2});
		Config->Groups.Add(Group);
		Points[0]->bRequired = true;
		Points[0]->AllowedClasses.Add(RareClass);
		auto Accept = [](const FHBChestSpawnChoice&, FString&) { return true; };
		auto Signature = [](const TArray<FHBChestSpawnChoice>& Choices)
		{
			FString Result;
			for (const FHBChestSpawnChoice& Choice : Choices)
				Result += Choice.PointId.ToString() + TEXT(":") +
					GetNameSafe(Choice.ChestClass.Get()) + TEXT("|");
			return Result;
		};
		TArray<FHBChestSpawnChoice> Placed;
		FString Error;
		TestTrue(TEXT("Place the configured number of boxes"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("Fixed count"), Placed.Num(), 3);
		TestTrue(TEXT("Required point uses its allowed box class"), Placed.ContainsByPredicate(
			[&](const FHBChestSpawnChoice& Choice)
			{
				return Choice.PointId == Points[0]->PointId && Choice.ChestClass.Get() == RareClass;
			}));
		const FString FirstSignature = Signature(Placed);
		Algo::Reverse(Points);
		TestTrue(TEXT("Reordered actor enumeration is accepted"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("The same dungeon seed and room replay point and class draws"),
			Signature(Placed), FirstSignature);
		Algo::Reverse(Points);
		TSet<FString> RoomSignatures;
		for (int32 RoomId = 0; RoomId < 8; ++RoomId)
		{
			TestTrue(TEXT("Each room can select independently"),
				HBChestSelection::SelectAndPlace(*Config, Points, 12345, RoomId, Accept, Placed, Error));
			RoomSignatures.Add(Signature(Placed));
		}
		TestTrue(TEXT("Different room IDs can change placement"), RoomSignatures.Num() > 1);
		Config->Groups[0].MinCount = 2;
		Config->Groups[0].MaxCount = 4;
		TSet<int32> Counts;
		TSet<FString> Signatures;
		for (int32 Seed = 1; Seed <= 32; ++Seed)
		{
			TestTrue(TEXT("Ranged selection remains placeable"),
				HBChestSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
			TestTrue(TEXT("Count stays within LD limits"), Placed.Num() >= 2 && Placed.Num() <= 4);
			Counts.Add(Placed.Num());
			Signatures.Add(Signature(Placed));
		}
		TestTrue(TEXT("Dungeon seed varies the count"), Counts.Num() > 1);
		TestTrue(TEXT("Dungeon seed varies point or class selection"), Signatures.Num() > 1);
		Config->Groups[0].MinCount = Config->Groups[0].MaxCount = 3;
		int32 CommonDraws = 0;
		int32 RareDraws = 0;
		for (int32 Seed = 1; Seed <= 128; ++Seed)
		{
			HBChestSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error);
			for (const FHBChestSpawnChoice& Choice : Placed)
				(Choice.ChestClass.Get() == RareClass ? RareDraws : CommonDraws)++;
		}
		TestTrue(TEXT("Both weighted box classes can be drawn"), CommonDraws > 0 && RareDraws > 0);
		TestTrue(TEXT("The higher class weight is drawn more often across seeds"), RareDraws > CommonDraws);
		FHBChestPlacementGroup Bonus;
		Bonus.GroupId = TEXT("Bonus");
		Bonus.MinCount = Bonus.MaxCount = 1;
		Bonus.Chests.Add({CommonClass, 1});
		Config->Groups.Add(Bonus);
		TestTrue(TEXT("Two groups can share room points"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TSet<FName> UsedPointIds;
		for (const FHBChestSpawnChoice& Choice : Placed) UsedPointIds.Add(Choice.PointId);
		TestEqual(TEXT("No spawn point is used twice across groups"), UsedPointIds.Num(), 4);
		Config->Groups.Pop();
		Points[1]->GroupId = TEXT("OtherGroup");
		TestTrue(TEXT("Another group's point is excluded"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestFalse(TEXT("Mismatched group point is not chosen"), Placed.ContainsByPredicate(
			[&](const FHBChestSpawnChoice& Choice) { return Choice.PointId == Points[1]->PointId; }));
		Points[1]->GroupId = NAME_None;
		auto RejectRare = [RareClass](const FHBChestSpawnChoice& Choice, FString& Reason)
		{
			if (Choice.ChestClass.Get() != RareClass) return true;
			Reason = TEXT("Rare class rejected by placement");
			return false;
		};
		TestFalse(TEXT("A required rare-only point cannot silently use another class"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectRare, Placed, Error));
		TestTrue(TEXT("Required point failure is reported"), Error.Contains(TEXT("required chest point")));
		Points[0]->bRequired = false;
		TestTrue(TEXT("Rejected class falls back to another class or point"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectRare, Placed, Error));
		TestEqual(TEXT("Fallback keeps the configured minimum"), Placed.Num(), 3);
		TestTrue(TEXT("Only accepted box classes are selected"), !Placed.ContainsByPredicate(
			[RareClass](const FHBChestSpawnChoice& Choice) { return Choice.ChestClass.Get() == RareClass; }));
		Points[1]->PointId = Points[2]->PointId;
		TestFalse(TEXT("Duplicate point IDs are rejected"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestTrue(TEXT("Duplicate ID explains the failure"), Error.Contains(TEXT("unique")));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
