#include "PCGModule/HBChestSpawnSubsystem.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBSpawnDiagnostics.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "Loot/BoxActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Components/BoxComponent.h"
#include "PCGModule/HBSpawnGroundQuery.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "CollisionQueryParams.h"

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
		TEXT("/Game/Loot/Blueprints/BP_Box_Rare.BP_Box_Rare_C"),
		TEXT("/Game/Loot/Blueprints/BP_Box_Epic.BP_Box_Epic_C"),
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
		TestTrue(TEXT("Existing chest groups require entry paths by default"), Config->Groups[0].bRequireEntryPath);
		TestFalse(TEXT("Default policy reaches every chest choice"),
			Placed.ContainsByPredicate([](const FHBChestSpawnChoice& Choice) { return !Choice.bRequireEntryPath; }));
		Config->Groups[0].bRequireEntryPath = false;
		TestTrue(TEXT("A locked-area chest group can skip entry paths"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("Changing path policy preserves seeded count, point and class draws"),
			Signature(Placed), FirstSignature);
		TestFalse(TEXT("Disabled policy reaches every choice in that group"),
			Placed.ContainsByPredicate([](const FHBChestSpawnChoice& Choice) { return Choice.bRequireEntryPath; }));
		Config->Groups[0].bRequireEntryPath = true;
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
		Config->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
		Config->Groups[0].CountWeights = {{2, 1}, {3, 0}, {4, 5}};
		int32 TwoCount = 0, FourCount = 0;
		for (int32 Seed = 1; Seed <= 64; ++Seed)
		{
			TestTrue(TEXT("Chest selection accepts weighted counts"),
				HBChestSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
			TestTrue(TEXT("Chest count uses only positive-weight choices"), Placed.Num() == 2 || Placed.Num() == 4);
			TwoCount += Placed.Num() == 2;
			FourCount += Placed.Num() == 4;
		}
		TestTrue(TEXT("Larger chest count follows its greater weight"), FourCount > TwoCount && TwoCount > 0);
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error);
		const FString WeightedSignature = Signature(Placed);
		Algo::Reverse(Config->Groups[0].CountWeights);
		TestTrue(TEXT("Weighted chest selection replays"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("Weighted chest replay preserves points and classes"), Signature(Placed), WeightedSignature);
		Config->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Uniform;
		Config->Groups[0].CountWeights.Reset();
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
		Bonus.bRequireEntryPath = false;
		Config->Groups.Add(Bonus);
		TestTrue(TEXT("Two groups can share room points"),
			HBChestSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TSet<FName> UsedPointIds;
		for (const FHBChestSpawnChoice& Choice : Placed) UsedPointIds.Add(Choice.PointId);
		TestEqual(TEXT("No spawn point is used twice across groups"), UsedPointIds.Num(), 4);
		TestEqual(TEXT("Only the group configured to skip entry paths does so"),
			Placed.FilterByPredicate([](const FHBChestSpawnChoice& Choice) { return !Choice.bRequireEntryPath; }).Num(), 1);
		TestEqual(TEXT("The other group retains its entry path requirement"),
			Placed.FilterByPredicate([](const FHBChestSpawnChoice& Choice) { return Choice.bRequireEntryPath; }).Num(), 3);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBChestFailureDiagnosticsTest,
	"HeavenBreach.PCGModule.ChestSpawn.FailureDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBChestFailureDiagnosticsTest::RunTest(const FString&)
{
	UClass* NativeClass = ABoxActor::StaticClass();
	UClass* RareClass = LoadClass<ABoxActor>(nullptr, TEXT("/Game/Loot/Blueprints/BP_Box_Rare.BP_Box_Rare_C"));
	if (!TestNotNull(TEXT("Existing rare class is available for class attribution"), RareClass)) return false;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<AHBChestSpawnPoint*> Points;
	for (const TCHAR* Id : {TEXT("A_Required"), TEXT("B_Required"), TEXT("C_FirstFill"), TEXT("D_LastFill")})
	{
		auto* Point = World->SpawnActor<AHBChestSpawnPoint>();
		Point->PointId = FName(Id);
		Point->GroupId = Points.Num() == 0 || Points.Num() == 2 ? TEXT("First") : TEXT("Last");
		Point->bRequired = Points.Num() < 2;
		if (!Points.IsEmpty()) Point->AllowedClasses.Add(NativeClass);
		Points.Add(Point);
	}
	UHBChestPlacementComponent* Config = NewObject<UHBChestPlacementComponent>(GetTransientPackage());
	FHBChestPlacementGroup First;
	First.GroupId = TEXT("First");
	First.MinCount = First.MaxCount = 1;
	First.Chests = {{NativeClass, 1}, {RareClass, 2}};
	FHBChestPlacementGroup Last = First;
	Last.GroupId = TEXT("Last");
	Config->Groups = {First, Last};
	TArray<FString> ConfigErrors;
	if (!TestTrue(TEXT("Diagnostic fixture has valid chest settings"), Config->Validate(ConfigErrors)))
	{
		AddError(FString::Join(ConfigErrors, TEXT("; ")));
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	TArray<FHBChestSpawnChoice> Placed;
	FString Error;
	FHBSpawnSelectionReport Report;
	int32 Calls = 0;
	bool bRejectB = true;
	auto RejectRequired = [&](const FHBChestSpawnChoice& Choice, FString& Reason)
	{
		++Calls;
		if (Choice.Point == Points[0])
		{
			Reason = Choice.ChestClass.Get() == NativeClass ? TEXT("A-native-floor") : TEXT("A-rare-collision");
			return false;
		}
		if (bRejectB && Choice.Point == Points[1]) { Reason = TEXT("B-approach-path"); return false; }
		return true;
	};
	TestFalse(TEXT("Other chest points cannot substitute for required A"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("Required chest failure has its own typed reason"),
		Report.FailureReason == EHBSpawnFailureReason::RequiredPointUnfilled && Report.FailedPointId == Points[0]->PointId);
	TestEqual(TEXT("Chest diagnostics add no placement attempts"), Calls, 5);
	TestEqual(TEXT("Other chests satisfy both group minima"), Placed.Num(), 2);
	TestTrue(TEXT("Required chest error contains both of A's class failures"), Error.Contains(TEXT("A-native-floor")) && Error.Contains(TEXT("A-rare-collision")));
	TestFalse(TEXT("A never borrows B's chest error"), Error.Contains(TEXT("B-approach-path")));
	TestEqual(TEXT("Chest class failures remain associated with A"), Report.Points.FindChecked(Points[0]->PointId).ClassFailures.Num(), 2);
	TestTrue(TEXT("Both chest class paths are reported"), Error.Contains(NativeClass->GetPathName()) && Error.Contains(RareClass->GetPathName()));
	bRejectB = false;
	Calls = 0;
	TestFalse(TEXT("Required A still reports its cause after B succeeds"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestEqual(TEXT("Reused chest report drops B's old failure"), Report.Points.FindChecked(Points[1]->PointId).ClassFailures.Num(), 0);
	TestEqual(TEXT("Each rejected chest class is attempted once"), Calls, 4);
	auto RejectLastGroup = [&](const FHBChestSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.Point->GroupId == Last.GroupId) { Reason = TEXT("Last-group-spacing"); return false; }
		return RejectRequired(Choice, Reason);
	};
	TestFalse(TEXT("Chest minimum failure keeps priority over final required checks"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectLastGroup, Placed, Error, &Report));
	TestTrue(TEXT("Chest minimum failure identifies its own group"),
		Report.FailureReason == EHBSpawnFailureReason::MinCountNotMet && Report.FailedGroupId == Last.GroupId && Report.FailedPointId.IsNone());
	TestTrue(TEXT("Chest group failure excludes other groups' details"), Error.Contains(TEXT("Last-group-spacing")) && !Error.Contains(TEXT("A-native-floor")));
	TestTrue(TEXT("Chest minimum, target and placed counts are recorded"),
		Report.Groups.Last().MinCount == 1 && Report.Groups.Last().Target == 1 && Report.Groups.Last().Placed == 0);
	Config->Groups[0].Chests = {{NativeClass, 1}};
	Points[0]->AllowedClasses = {RareClass};
	Calls = 0;
	TestFalse(TEXT("An incompatible required chest stays unfilled"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("No compatible chest class is explicit"), Error.Contains(TEXT("NoCompatibleClass")) && !Error.Contains(TEXT("A-native-floor")));
	TestEqual(TEXT("No placement call is made for an incompatible chest point"), Calls, 2);
	TestTrue(TEXT("No chest attempt is fabricated"), Report.Points.FindChecked(Points[0]->PointId).ClassFailures.IsEmpty());
	Points[0]->AllowedClasses.Reset();
	Points[0]->GroupId = TEXT("MissingGroup");
	TestFalse(TEXT("A mismatched required chest group fails"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("Unattempted chest group mismatch is explicit"), Error.Contains(TEXT("GroupMismatch")));
	Points[0]->GroupId = First.GroupId;
	Points[2]->bRequired = true;
	auto Accept = [](const FHBChestSpawnChoice&, FString&) { return true; };
	TestFalse(TEXT("Chest target does not waive required points"),
		HBChestSelection::SelectAndPlace(*Config, Points, 12345, 11, Accept, Placed, Error, &Report));
	TestTrue(TEXT("A target-skipped required chest is explicit"), Error.Contains(TEXT("TargetReached")));
	TestTrue(TEXT("No failure is invented for an unattempted chest"), Report.Points.FindChecked(Report.FailedPointId).ClassFailures.IsEmpty());
	Points[2]->bRequired = false;
	Config->Groups[0] = First;
	auto Signature = [](const TArray<FHBChestSpawnChoice>& Choices)
	{
		FString Result;
		for (const auto& Choice : Choices) Result += Choice.PointId.ToString() + TEXT(":") + Choice.ChestClass->GetPathName() + TEXT("|");
		return Result;
	};
	for (int32 Seed = 1; Seed <= 8; ++Seed)
	{
		TestTrue(TEXT("Legacy chest call succeeds"), HBChestSelection::SelectAndPlace(*Config, Points, Seed, 11, Accept, Placed, Error));
		const FString Expected = Signature(Placed);
		TestTrue(TEXT("Optional chest report succeeds"), HBChestSelection::SelectAndPlace(*Config, Points, Seed, 11, Accept, Placed, Error, &Report));
		TestEqual(TEXT("Chest report preserves seeded class, point and quantity draws"), Signature(Placed), Expected);
		TestTrue(TEXT("Successful chest reuse clears aggregate failures"), Report.FailureReason == EHBSpawnFailureReason::None && Report.FailedPointId.IsNone());
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBChestPlacementFailureDiagnosticsTest,
	"HeavenBreach.PCGModule.ChestSpawn.PlacementFailureDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBChestPlacementFailureDiagnosticsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* FloorActor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBox = NewObject<UBoxComponent>(FloorActor);
	FloorActor->SetRootComponent(FloorBox);
	FloorBox->SetBoxExtent(FVector(500, 500, 10));
	FloorBox->SetCollisionProfileName(TEXT("BlockAll"));
	FloorBox->SetCanEverAffectNavigation(false);
	FloorBox->RegisterComponent();
	FloorActor->SetActorLocation(FVector(0, 0, -10));
	auto* Setup = World->SpawnActor<AHBChestSetupActor>();
	auto* Required = World->SpawnActor<AHBChestSpawnPoint>();
	Required->PointId = TEXT("A_Required");
	Required->bRequired = true;
	Required->SetActorLocation(FVector(0, 0, 0.4));
	FHBChestPlacementGroup Group;
	Group.GroupId = TEXT("Field");
	Group.MinCount = Group.MaxCount = 1;
	Group.Chests = {{ABoxActor::StaticClass(), 1}};
	Required->GroupId = Group.GroupId;
	Setup->Placement->Groups = {Group};
	const FBox Bounds(FVector(-500, -500, -200), FVector(500, 500, 300));
	FString Error;
	UHBChestSpawnSubsystem* Subsystem = World->GetSubsystem<UHBChestSpawnSubsystem>();
	if (TestNotNull(TEXT("Chest subsystem exists in a physical room fixture"), Subsystem))
	{
		TestFalse(TEXT("Actual chest approach rejects a room without navigation"),
			Subsystem->StartPlacement(1, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error));
		TestTrue(TEXT("Actual chest navigation failures retain approach extent and exact missing prerequisite"),
			Error.Contains(TEXT("NavProjectionFailed:25")) && Error.Contains(TEXT("Stage=ChestApproach"))
			&& Error.Contains(TEXT("Condition=MissingSystem")) && Error.Contains(TEXT("Extent=X=100.000 Y=100.000 Z=200.000"))
			&& Error.Contains(TEXT("ProjectionQueried=0")) && Error.Contains(TEXT("FailureReason=MinCountNotMet")));
		TestFalse(TEXT("Actual chest entry spacing rejects before navigation"),
			Subsystem->StartPlacement(2, 836511139, 11, World->PersistentLevel, Bounds, {FVector::ZeroVector}, Error));
		TestTrue(TEXT("Chest entry spacing is separately counted"), Error.Contains(TEXT("EntrySpacing:25")));
		auto* Exclusion = World->SpawnActor<AHBRoomSpawnExclusionVolume>();
		Exclusion->Bounds->SetBoxExtent(FVector(100, 100, 200));
		TestFalse(TEXT("Authored chest exclusion is still applied"),
			Subsystem->StartPlacement(3, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error));
		TestTrue(TEXT("Chest exclusion is separately counted"), Error.Contains(TEXT("ExcludedArea:25")));
		Exclusion->SetActorLocation(FVector(10000, 0, 0));
		Required->SetActorLocation(FVector(1000, 0, 0.4));
		TestFalse(TEXT("Chest marker outside the room still rejects before ground search"),
			Subsystem->StartPlacement(4, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error));
		TestTrue(TEXT("Pre-search bounds failure does not invent 25 candidates"),
			Error.Contains(TEXT("OutsideRoom:1")) && Error.Contains(TEXT("CandidatesChecked=0")));
		Required->SetActorLocation(FVector(0, 0, 0.4));
	}

	// Drive the public typed selection callback with real ground queries, independent of runtime navigation.
	UClass* RareClass = LoadClass<ABoxActor>(nullptr, TEXT("/Game/Loot/Blueprints/BP_Box_Rare.BP_Box_Rare_C"));
	if (TestNotNull(TEXT("Existing rare chest class is available"), RareClass))
	{
		FloorActor->SetActorLocation(FVector(20, 0, -10));
		FloorBox->SetBoxExtent(FVector(15, 500, 10));
		Group.Chests.Add({RareClass, 1});
		Setup->Placement->Groups = {Group};
		auto* Optional = World->SpawnActor<AHBChestSpawnPoint>();
		Optional->PointId = TEXT("Z_Optional");
		Optional->GroupId = Group.GroupId;
		Optional->AllowedClasses = {ABoxActor::StaticClass()};
		FCollisionQueryParams Query(SCENE_QUERY_STAT(HBChestTypedSelectionFixture), false);
		FHBSpawnSelectionReport Report;
		TArray<FHBChestSpawnChoice> Placed;
		auto TryGround = [&](const FHBChestSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport& Evidence)
		{
			FHitResult Floor;
			return HBSpawnGroundQuery::FindPlacementWithDiagnostics(*World, FVector(0, 0, 0.4), FQuat::Identity,
				Bounds, 0.7f, Query, [&](const FHitResult&, FString& CandidateReason, FHBSpawnCandidateReport& Candidate)
				{
					if (Choice.Point == Optional) return true;
					Candidate.Cause = Choice.ChestClass.Get() == ABoxActor::StaticClass()
						? EHBSpawnFailureReason::ExcludedArea : EHBSpawnFailureReason::NavProjectionFailed;
					CandidateReason = TEXT("Opaque detail intentionally has no reason enum name");
					return false;
				}, Floor, Reason, Evidence);
		};
		TestFalse(TEXT("Required chest retains both classes' mixed ground and placement causes"),
			HBChestSelection::SelectAndPlaceWithDiagnostics(*Setup->Placement, {Required, Optional}, 836511139, 11,
				TryGround, Placed, Error, &Report));
		TestTrue(TEXT("Required and minimum policy stay separate"),
			Report.FailureReason == EHBSpawnFailureReason::RequiredPointUnfilled && Placed.Num() == 1);
		const auto RequiredCounts = Report.CountFailureReasons(Required->PointId);
		TestEqual(TEXT("Required chest counts floors independently for each class"), RequiredCounts.FindRef(EHBSpawnFailureReason::NoFloor), 32);
		TestEqual(TEXT("Native chest retains its own rejection cause"), RequiredCounts.FindRef(EHBSpawnFailureReason::ExcludedArea), 9);
		TestEqual(TEXT("Rare chest retains its own rejection cause"), RequiredCounts.FindRef(EHBSpawnFailureReason::NavProjectionFailed), 9);
		const auto GroupCounts = Report.CountFailureReasons(NAME_None, Group.GroupId);
		TestEqual(TEXT("Successful optional correction retains its earlier missing floor"), GroupCounts.FindRef(EHBSpawnFailureReason::NoFloor), 33);
		TestEqual(TEXT("Successful class report is retained separately from class failures"),
			Report.Points.FindChecked(Optional->PointId).ClassPlacements.Num(), 1);
		TestFalse(TEXT("Required output cannot borrow optional correction counts"), Error.Contains(TEXT("NoFloor:33")));
		for (const FHBSpawnClassFailure& Failure : Report.Points.FindChecked(Required->PointId).ClassFailures)
		{
			const EHBSpawnFailureReason OwnCause = Failure.ClassPath == ABoxActor::StaticClass()->GetPathName()
				? EHBSpawnFailureReason::ExcludedArea : EHBSpawnFailureReason::NavProjectionFailed;
			TestEqual(TEXT("Each chest class keeps its own nine placement failures"), Failure.Placement.CountFailureReasons().FindRef(OwnCause), 9);
			TestEqual(TEXT("Each chest class keeps all 25 original candidate indices"), Failure.Placement.Candidates.Num(), 25);
		}
		Group.MinCount = Group.MaxCount = 2;
		Setup->Placement->Groups = {Group};
		TestFalse(TEXT("Typed chest minimum failure remains earlier than required validation"),
			HBChestSelection::SelectAndPlaceWithDiagnostics(*Setup->Placement, {Required, Optional}, 836511139, 11,
				TryGround, Placed, Error, &Report));
		TestTrue(TEXT("Minimum error contains the exact group total including successful corrections"),
			Report.FailureReason == EHBSpawnFailureReason::MinCountNotMet && Error.Contains(TEXT("NoFloor:33")));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
