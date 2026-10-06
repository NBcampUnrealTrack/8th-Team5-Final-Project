#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBSpawnDiagnostics.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "AI/EnemyCharacterBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Navigation/NavLinkProxy.h"
#include "NavigationPath.h"
#include "UObject/Package.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterMonsterSpacingTest, "HeavenBreach.PCGModule.Encounter.MonsterSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterMonsterSpacingTest::RunTest(const FString&)
{
	const FVector Out8(-3883.542, 1460.838, 0.);
	const FVector Out9(-3776.357, 1281.164, 395.315);
	TestFalse(TEXT("Adventure1-1 Out8 and Out9 on separate floors do not block each other"),
		HBEncounterPlacement::AreCapsulesTooClose(Out8, 80.f, 90.f, Out9, 34.f, 88.f, 120.f));
	TestFalse(TEXT("Separated floors remain valid when the spawn order is reversed"),
		HBEncounterPlacement::AreCapsulesTooClose(Out9, 34.f, 88.f, Out8, 80.f, 90.f, 120.f));
	TestTrue(TEXT("The same horizontal positions still require spacing on the same floor"),
		HBEncounterPlacement::AreCapsulesTooClose(Out8, 80.f, 90.f,
			FVector(Out9.X, Out9.Y, Out8.Z), 34.f, 88.f, 120.f));
	TestTrue(TEXT("Partially overlapping vertical extents retain spacing even at a large 3D distance"),
		HBEncounterPlacement::AreCapsulesTooClose(Out8, 80.f, 90.f,
			FVector(Out9.X, Out9.Y, 177.), 34.f, 88.f, 120.f));
	TestFalse(TEXT("Capsules stacked with separated vertical extents are allowed"),
		HBEncounterPlacement::AreCapsulesTooClose(FVector::ZeroVector, 80.f, 90.f,
			FVector(0., 0., 179.), 34.f, 88.f, 120.f));
	TestFalse(TEXT("Capsules whose vertical extents only touch do not block spacing"),
		HBEncounterPlacement::AreCapsulesTooClose(FVector::ZeroVector, 80.f, 90.f,
			FVector(0., 0., 178.), 34.f, 88.f, 120.f));
	TestFalse(TEXT("A capsule on the lower floor is also allowed"),
		HBEncounterPlacement::AreCapsulesTooClose(FVector::ZeroVector, 80.f, 90.f,
			FVector(0., 0., -179.), 34.f, 88.f, 120.f));
	TestTrue(TEXT("A taller enemy still blocks when its capsule reaches the other floor"),
		HBEncounterPlacement::AreCapsulesTooClose(Out8, 80.f, 90.f, Out9, 34.f, 400.f, 120.f));
	TestTrue(TEXT("Same-floor spacing rejects positions inside the minimum horizontal distance"),
		HBEncounterPlacement::AreCapsulesTooClose(FVector::ZeroVector, 80.f, 90.f,
			FVector(233., 0., 0.), 34.f, 88.f, 120.f));
	TestFalse(TEXT("Same-floor spacing allows the exact minimum horizontal distance"),
		HBEncounterPlacement::AreCapsulesTooClose(FVector::ZeroVector, 80.f, 90.f,
			FVector(234., 0., 0.), 34.f, 88.f, 120.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterSelectionTest, "HeavenBreach.PCGModule.EncounterSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterSelectionTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<AHBEnemySpawnPoint*> Points;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto* Point = World->SpawnActor<AHBEnemySpawnPoint>();
		Point->PointId = FName(*FString::Printf(TEXT("Point_%02d"), Index));
		Points.Add(Point);
	}
	Points[0]->bRequired = true;
	UHBEncounterComponent* Config = NewObject<UHBEncounterComponent>(GetTransientPackage());
	FHBEncounterGroup Common;
	Common.GroupId = TEXT("Common");
	Common.MinCount = Common.MaxCount = 3;
	FHBEncounterMonsterOption Option;
	Option.EnemyClass = AEnemyCharacterBase::StaticClass();
	Common.Monsters.Add(Option);
	Config->Groups.Add(Common);
	TArray<FHBEncounterSpawnChoice> Placed;
	FString Error;
	auto Accept = [](const FHBEncounterSpawnChoice&, FString&) { return true; };
	auto Signature = [](const TArray<FHBEncounterSpawnChoice>& Choices)
	{
		FString Result;
		for (const FHBEncounterSpawnChoice& Choice : Choices) Result += Choice.PointId.ToString() + TEXT("|");
		return Result;
	};
	TestTrue(TEXT("Select three common monsters"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exact requested count"), Placed.Num(), 3);
	TestTrue(TEXT("Required point selected"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[0]->PointId; }));
	const FString FirstSignature = Signature(Placed);
	Algo::Reverse(Points);
	TestTrue(TEXT("Reordered actor enumeration accepted"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Same seed and room yield the same placement"), Signature(Placed), FirstSignature);
	{
		const TArray<FHBEncounterGroup> SavedGroups = Config->Groups;
		TestTrue(TEXT("Existing groups require entry paths by default"), Config->Groups[0].bRequireEntryPath);
		TestTrue(TEXT("Default entry path policy reaches every spawn choice"),
			!Placed.ContainsByPredicate([](const FHBEncounterSpawnChoice& Choice) { return !Choice.bRequireEntryPath; }));
		Config->Groups[0].bRequireEntryPath = false;
		TestTrue(TEXT("A locked-area group can disable the entry path requirement"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("Changing path policy does not consume seed draws"), Signature(Placed), FirstSignature);
		TestFalse(TEXT("Disabled policy reaches every choice in that group"),
			Placed.ContainsByPredicate([](const FHBEncounterSpawnChoice& Choice) { return Choice.bRequireEntryPath; }));

		Config->Groups = SavedGroups;
		FHBEncounterGroup Locked = Common;
		Locked.GroupId = TEXT("Locked");
		Locked.MinCount = Locked.MaxCount = 1;
		Locked.bRequireEntryPath = false;
		Config->Groups.Add(Locked);
		TestTrue(TEXT("Open and locked groups can share unassigned points"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
		TestEqual(TEXT("Only the locked group skips entry paths"),
			Placed.FilterByPredicate([](const FHBEncounterSpawnChoice& Choice) { return !Choice.bRequireEntryPath; }).Num(), 1);
		TestEqual(TEXT("The open group keeps its entry path requirement"),
			Placed.FilterByPredicate([](const FHBEncounterSpawnChoice& Choice) { return Choice.bRequireEntryPath; }).Num(), 3);
		Config->Groups = SavedGroups;
	}
	TSet<FString> RoomSignatures;
	for (int32 RoomId = 0; RoomId < 8; ++RoomId)
	{
		TestTrue(TEXT("Reused module plans independently for each room"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, RoomId, Accept, Placed, Error));
		RoomSignatures.Add(Signature(Placed));
	}
	TestTrue(TEXT("Different room IDs can change placement"), RoomSignatures.Num() > 1);
	Config->Groups[0].MinCount = 2;
	Config->Groups[0].MaxCount = 4;
	TSet<int32> SeenCounts;
	for (int32 Seed = 1; Seed <= 32; ++Seed)
	{
		TestTrue(TEXT("Ranged count remains placeable"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
		TestTrue(TEXT("Ranged count stays within LD limits"), Placed.Num() >= 2 && Placed.Num() <= 4);
		SeenCounts.Add(Placed.Num());
	}
	TestTrue(TEXT("Changing dungeon seed can vary the count"), SeenCounts.Num() > 1);
	Config->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
	Config->Groups[0].CountWeights = {{2, 1}, {3, 0}, {4, 5}};
	int32 TwoCount = 0, FourCount = 0;
	for (int32 Seed = 1; Seed <= 64; ++Seed)
	{
		TestTrue(TEXT("Monster selection accepts weighted counts"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
		TestTrue(TEXT("Monster count uses only positive-weight choices"), Placed.Num() == 2 || Placed.Num() == 4);
		TwoCount += Placed.Num() == 2;
		FourCount += Placed.Num() == 4;
	}
	TestTrue(TEXT("Larger monster count follows its greater weight"), FourCount > TwoCount && TwoCount > 0);
	HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error);
	const FString WeightedSignature = Signature(Placed);
	Algo::Reverse(Config->Groups[0].CountWeights);
	TestTrue(TEXT("Weighted monster selection replays"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Weighted monster replay preserves points"), Signature(Placed), WeightedSignature);
	Config->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Uniform;
	Config->Groups[0].CountWeights.Reset();
	Config->Groups[0].MinCount = Config->Groups[0].MaxCount = 3;
	Points[1]->GroupId = TEXT("AnotherGroup");
	TestTrue(TEXT("Other group point is not selected"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestFalse(TEXT("Group filtering excludes the marked point"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[1]->PointId; }));
	Points[1]->GroupId = NAME_None;

	{
		const TArray<FHBEncounterGroup> SavedGroups = Config->Groups;
		TArray<bool> SavedRequired, SavedElite;
		for (AHBEnemySpawnPoint* Point : Points)
		{
			SavedRequired.Add(Point->bRequired);
			SavedElite.Add(Point->bAllowElite);
			Point->bRequired = false;
		}
		Points[2]->bAllowElite = true;
		FHBEncounterGroup KeyElite;
		KeyElite.GroupId = TEXT("KeyElite");
		KeyElite.Grade = EHBEncounterGrade::Elite;
		KeyElite.MinCount = 0;
		KeyElite.MaxCount = 1;
		KeyElite.CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
		KeyElite.CountWeights = {{0, 1}, {1, 1}};
		KeyElite.Monsters.Add(Option);
		FHBEncounterGroup KeyCommon = Common;
		KeyCommon.GroupId = TEXT("KeyCommon");
		KeyCommon.MinCount = 0;
		KeyCommon.MaxCount = 2;
		KeyCommon.CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
		KeyCommon.CountWeights = {{2, 1}};
		KeyCommon.ConditionGroupId = KeyElite.GroupId;
		KeyCommon.CountWeightsIfConditionSpawned = {{1, 1}};
		Config->Groups = {KeyCommon, KeyElite};
		int32 WithElite = 0, WithoutElite = 0;
		for (int32 Seed = 1; Seed <= 64; ++Seed)
		{
			TestTrue(TEXT("Conditional count selection succeeds"),
				HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
			const int32 Elites = Placed.FilterByPredicate(
				[](const FHBEncounterSpawnChoice& Choice) { return Choice.Grade == EHBEncounterGrade::Elite; }).Num();
			TestEqual(TEXT("Conditional count keeps the room total"), Placed.Num(), 2);
			WithElite += Elites == 1;
			WithoutElite += Elites == 0;
		}
		TestTrue(TEXT("Both condition outcomes occur"), WithElite > 0 && WithoutElite > 0);
		TArray<FString> ConditionErrors;
		Config->Groups[0].ConditionGroupId = TEXT("Missing");
		TestFalse(TEXT("Unknown condition group is rejected"), Config->Validate(ConditionErrors));
		Config->Groups[0].ConditionGroupId = KeyElite.GroupId;
		Config->Groups[1].Grade = EHBEncounterGrade::Common;
		TestFalse(TEXT("Condition group placed later is rejected"), Config->Validate(ConditionErrors));
		Config->Groups = SavedGroups;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Points[Index]->bRequired = SavedRequired[Index];
			Points[Index]->bAllowElite = SavedElite[Index];
		}
	}

	FName RejectedPoint;
	const FName RequiredId = Points.Last()->PointId;
	auto RejectOne = [&RejectedPoint, RequiredId](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.PointId == RequiredId) return true;
		if (RejectedPoint.IsNone()) RejectedPoint = Choice.PointId;
		if (Choice.PointId == RejectedPoint) { Reason = TEXT("Unsafe candidate"); return false; }
		return true;
	};
	TestTrue(TEXT("Rejected point is replaced"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectOne, Placed, Error));
	TestEqual(TEXT("Minimum survives one rejection"), Placed.Num(), 3);
	TestFalse(TEXT("Rejected point is absent"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == RejectedPoint; }));
	auto RejectAll = [](const FHBEncounterSpawnChoice&, FString& Reason)
	{
		Reason = TEXT("No safe floor");
		return false;
	};
	TestFalse(TEXT("Insufficient required count fails"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectAll, Placed, Error));
	TestTrue(TEXT("Failure names room and reason"), Error.Contains(TEXT("Room 2")) && Error.Contains(TEXT("No safe floor")));

	Config->Groups.Reset();
	FHBEncounterGroup Boss;
	Boss.GroupId = TEXT("Boss");
	Boss.Grade = EHBEncounterGrade::Boss;
	Boss.MinCount = Boss.MaxCount = 1;
	Boss.Monsters.Add(Option);
	Config->Groups.Add(Boss);
	Points.Last()->bAllowCommon = false;
	Points.Last()->bAllowBoss = true;
	Points.Last()->GroupId = TEXT("Boss");
	TestTrue(TEXT("Boss uses its dedicated point group"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exactly one boss"), Placed.Num(), 1);
	if (Placed.Num() == 1)
	{
		TestEqual(TEXT("Boss point ID"), Placed[0].PointId, Points.Last()->PointId);
		TestEqual(TEXT("Boss grade"), Placed[0].Grade, EHBEncounterGrade::Boss);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterCapsuleBlockingDiagnosticsTest,
	"HeavenBreach.PCGModule.Encounter.CapsuleBlockingDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterCapsuleBlockingDiagnosticsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto AddBox = [&](const TCHAR* Name, const FVector& Location, const FVector& Extent,
		FName Profile, AActor* Owner = nullptr)
	{
		if (!Owner)
		{
			FActorSpawnParameters Params;
			Params.Name = FName(Name);
			Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Owner, FName(Name));
		if (Owner->GetRootComponent()) Box->SetupAttachment(Owner->GetRootComponent());
		else Owner->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(Profile);
		Box->SetCanEverAffectNavigation(false);
		Box->RegisterComponent();
		Box->SetWorldLocation(Location);
		return Box;
	};
	UBoxComponent* FloorBox = AddBox(TEXT("FixtureFloor"), FVector(0, 0, -10), FVector(500, 500, 10), TEXT("BlockAll"));
	UBoxComponent* WallBox = AddBox(TEXT("FixtureWall"), FVector(20, 0, 60), FVector(4, 80, 60), TEXT("BlockAll"));
	UBoxComponent* WallSecondary = AddBox(TEXT("WallSecondary"), FVector(-20, 0, 60), FVector(4, 80, 60), TEXT("BlockAll"), WallBox->GetOwner());
	UBoxComponent* OtherWall = AddBox(TEXT("OtherBlockingActor"), FVector(0, 25, 60), FVector(4, 4, 60), TEXT("BlockAll"));
	UBoxComponent* TouchBox = AddBox(TEXT("TouchOnly"), FVector(-10, -15, 60), FVector(4, 4, 10), TEXT("OverlapAll"));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBCapsuleBlockingDiagnostics), false);
	const FVector Marker(0, 0, 0.4);
	FHitResult Floor;
	if (!TestTrue(TEXT("Fixture floor is found by the original floor trace"),
		World->LineTraceSingleByChannel(Floor, Marker + FVector(0, 0, 100), Marker - FVector(0, 0, 200), ECC_WorldStatic, Query)))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	TestEqual(TEXT("The floor trace identifies the floor, not a wall"), Floor.GetActor(), FloorBox->GetOwner());
	const FVector SpawnLocation = Floor.ImpactPoint + FVector(0, 0, 90);
	const FQuat Rotation = FRotator(0, 30, 0).Quaternion();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(34.f, 88.f);
	const FName Profile(TEXT("Pawn"));
	const bool bOriginalBlocked = World->OverlapBlockingTestByProfile(SpawnLocation, Rotation, Profile, Shape, Query);
	if (!TestTrue(TEXT("Original Pawn capsule query is blocked by fixture geometry"), bOriginalBlocked))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	const FHBSpawnCapsuleFailure Failure = HBSpawnDiagnostics::CollectCapsuleFailure(
		*World, Marker, Floor, SpawnLocation, Rotation, Profile, Shape, Query, 0);
	TestTrue(TEXT("Multi collection agrees with the original blocking test"), Failure.bMultiHasBlocking && !Failure.HasQueryMismatch());
	TestEqual(TEXT("Two wall components and another actor are all reported"), Failure.BlockingTargets.Num(), 3);
	TestEqual(TEXT("Ground actor path remains distinct from blockers"), Failure.GroundActorPath, FloorBox->GetOwner()->GetPathName());
	TestEqual(TEXT("Ground component path is retained"), Failure.GroundComponentPath, FloorBox->GetPathName());
	TestTrue(TEXT("Capsule size, rotation, profile and locations match the original query"),
		Failure.Radius == 34.f && Failure.HalfHeight == 88.f && Failure.Profile == Profile
		&& Failure.Rotation.Equals(Rotation) && Failure.SpawnLocation.Equals(SpawnLocation) && Failure.Marker.Equals(Marker));
	TestFalse(TEXT("Neither ground nor touch-only objects are attributed as blockers"),
		Failure.BlockingTargets.ContainsByPredicate([&](const FHBSpawnBlockingTarget& Target)
		{ return Target.ActorPath == FloorBox->GetOwner()->GetPathName() || Target.ActorPath == TouchBox->GetOwner()->GetPathName(); }));
	TArray<FOverlapResult> RawOverlaps;
	TestTrue(TEXT("Raw multi query reports a blocking hit"), World->OverlapMultiByProfile(RawOverlaps, SpawnLocation, Rotation, Profile, Shape, Query));
	TestTrue(TEXT("Fixture includes a real nonblocking touch result"),
		RawOverlaps.ContainsByPredicate([&](const FOverlapResult& Overlap) { return Overlap.GetActor() == TouchBox->GetOwner() && !Overlap.bBlockingHit; }));
	TArray<FOverlapResult> RepeatedOverlaps = RawOverlaps;
	Algo::Reverse(RepeatedOverlaps);
	RepeatedOverlaps.Append(RawOverlaps);
	FHBSpawnCapsuleFailure Repeated = Failure;
	Repeated.SetOverlaps(true, RepeatedOverlaps);
	TestEqual(TEXT("Repeated component results do not inflate blocker counts"), Repeated.BlockingTargets.Num(), 3);
	TestEqual(TEXT("Query return order and duplicates do not change diagnostic output"), Repeated.Format(), Failure.Format());
	FHBSpawnPlacementReport Placement;
	Placement.CapsuleChecks = 2;
	Placement.CapsuleFailures = {Failure, Repeated};
	Placement.CapsuleFailures.Last().CapsuleCheckIndex = 1;
	const FString Summary = Placement.FormatBlockingSummary();
	TestTrue(TEXT("An actor with multiple components counts once per rejected check"),
		Summary.Contains(WallBox->GetOwner()->GetPathName() + TEXT(":2")) && !Summary.Contains(WallBox->GetOwner()->GetPathName() + TEXT(":4")));
	TestTrue(TEXT("Components are independently counted once per rejected check"), Summary.Contains(WallBox->GetPathName() + TEXT(":2")));
	TestTrue(TEXT("Summary explains multiple blockers and distinguishes capsule checks"),
		Summary.Contains(TEXT("CapsuleChecks=2 CapsuleOverlapRejects=2")) && Summary.Contains(TEXT("MultipleBlockersPerCheck=1")));
	FCollisionQueryParams IgnoredQuery = Query;
	IgnoredQuery.AddIgnoredActor(WallBox->GetOwner());
	TestTrue(TEXT("Original query still sees the unignored blocking actor"), World->OverlapBlockingTestByProfile(SpawnLocation, Rotation, Profile, Shape, IgnoredQuery));
	const FHBSpawnCapsuleFailure Ignored = HBSpawnDiagnostics::CollectCapsuleFailure(
		*World, Marker, Floor, SpawnLocation, Rotation, Profile, Shape, IgnoredQuery, 2);
	TestTrue(TEXT("Collection preserves the capsule query's exact ignore list"),
		Ignored.BlockingTargets.Num() == 1 && Ignored.BlockingTargets[0].ActorPath == OtherWall->GetOwner()->GetPathName() && !Ignored.HasQueryMismatch());
	IgnoredQuery.AddIgnoredActor(OtherWall->GetOwner());
	TestFalse(TEXT("Touch-only overlap cannot make the original query blocking"), World->OverlapBlockingTestByProfile(SpawnLocation, Rotation, Profile, Shape, IgnoredQuery));
	FHBSpawnCapsuleFailure Mismatch = Failure;
	Mismatch.SetOverlaps(false, {});
	TestTrue(TEXT("A failed diagnostic requery is explicitly marked as a mismatch"), Mismatch.HasQueryMismatch() && Mismatch.Format().Contains(TEXT("QueryMismatch=1")));
	TestTrue(TEXT("A missing blocker is not replaced with the ground actor"), Mismatch.BlockingTargets.IsEmpty());
	FOverlapResult Unresolved;
	Unresolved.bBlockingHit = true;
	Mismatch.SetOverlaps(true, {Unresolved});
	TestTrue(TEXT("Unresolved blocking objects are explicit without guessing the floor as a blocker"),
		Mismatch.UnresolvedBlockingResults == 1 && Mismatch.BlockingTargets.IsEmpty()
		&& Mismatch.Format().Contains(TEXT("UnresolvedBlockingResults=1")));
	TestEqual(TEXT("Collection leaves the original collision decision unchanged"),
		World->OverlapBlockingTestByProfile(SpawnLocation, Rotation, Profile, Shape, Query), bOriginalBlocked);

	UClass* NativeClass = AEnemyCharacterBase::StaticClass();
	UClass* BladeClass = LoadClass<AEnemyCharacterBase>(nullptr,
		TEXT("/Game/Monsters/Remnant_Blade/Blueprint/BP_Remnant_Blade.BP_Remnant_Blade_C"));
	if (TestNotNull(TEXT("Existing second enemy class loads"), BladeClass))
	{
		auto* Required = World->SpawnActor<AHBEnemySpawnPoint>();
		Required->PointId = TEXT("A_Required");
		Required->bRequired = true;
		auto* Optional = World->SpawnActor<AHBEnemySpawnPoint>();
		Optional->PointId = TEXT("Z_Optional");
		UHBEncounterComponent* Config = NewObject<UHBEncounterComponent>(GetTransientPackage());
		FHBEncounterGroup Group;
		Group.GroupId = TEXT("Field");
		Group.MinCount = Group.MaxCount = 1;
		Group.Monsters = {{NativeClass, 1}, {BladeClass, 1}};
		Config->Groups = {Group};
		TArray<FHBEncounterSpawnChoice> Placed;
		FString Error;
		FHBSpawnSelectionReport Report;
		TestFalse(TEXT("Typed capsule evidence remains attached to its required point"),
			HBEncounterSelection::SelectAndPlaceWithDiagnostics(*Config, {Required, Optional}, 836511139, 11,
				[&](const FHBEncounterSpawnChoice& Choice, FString& Reason, FHBSpawnPlacementReport& Evidence)
				{
					if (Choice.Point == Optional) return true;
					Evidence.CapsuleChecks = Choice.EnemyClass.Get() == NativeClass ? 2 : 1;
					FHBSpawnCandidateReport& Missing = Evidence.Candidates.AddDefaulted_GetRef();
					Missing.CandidateIndex = 0;
					Missing.Cause = EHBSpawnFailureReason::NoFloor;
					for (int32 Index = 0; Index < Evidence.CapsuleChecks; ++Index)
					{
						FHBSpawnCandidateReport& Candidate = Evidence.Candidates.AddDefaulted_GetRef();
						Candidate.CandidateIndex = Index + 1;
						Candidate.Cause = EHBSpawnFailureReason::CapsuleOverlap;
						Candidate.CapsuleCheckIndex = Index;
						FHBSpawnCapsuleFailure& CapsuleFailure = Evidence.CapsuleFailures.Add_GetRef(
							HBSpawnDiagnostics::CollectCapsuleFailure(*World, Marker, Floor, SpawnLocation, Rotation, Profile, Shape, Query, Index));
						CapsuleFailure.CandidateIndex = Candidate.CandidateIndex;
					}
					Reason = TEXT("Enemy capsule overlaps blocking geometry.");
					return false;
				}, Placed, Error, &Report));
		const FHBSpawnPointReport* PointReport = Report.Points.Find(Required->PointId);
		if (TestNotNull(TEXT("Required point has its own diagnostic report"), PointReport))
		{
			TestEqual(TEXT("Both enemy classes retain independent query reports"), PointReport->ClassFailures.Num(), 2);
			for (const FHBSpawnClassFailure& ClassFailure : PointReport->ClassFailures)
			{
				const int32 Expected = ClassFailure.ClassPath == NativeClass->GetPathName() ? 2 : 1;
				TestEqual(TEXT("Capsule checks are never combined across classes"), ClassFailure.Placement.CapsuleChecks, Expected);
				TestEqual(TEXT("Capsule failures are never combined across classes"), ClassFailure.Placement.CapsuleFailures.Num(), Expected);
			}
		}
		TestTrue(TEXT("Required failure contains classified capsule evidence and real blockers"),
			Error.Contains(TEXT("FailureReason=RequiredPointUnfilled")) && Error.Contains(TEXT("Cause=CapsuleOverlap"))
			&& Error.Contains(TEXT("GroundActor=FixtureFloor")) && Error.Contains(TEXT("BlockingActor=FixtureWall")));
		TestEqual(TEXT("Required monster point aggregates both classes' missing floor counts"),
			Report.CountFailureReasons(Required->PointId).FindRef(EHBSpawnFailureReason::NoFloor), 2);
		TestEqual(TEXT("Monster group counts each rejected capsule candidate once"),
			Report.CountFailureReasons(NAME_None, Group.GroupId).FindRef(EHBSpawnFailureReason::CapsuleOverlap), 3);

		// Exercise the actual subsystem path: every candidate that has a safe floor is blocked by the two walls.
		Optional->GroupId = TEXT("AnotherGroup");
		Required->GroupId = Group.GroupId;
		auto* Setup = World->SpawnActor<AHBEncounterSetupActor>();
		Group.Monsters = {{NativeClass, 1}};
		Setup->Encounter->Groups = {Group};
		const FBox Bounds(FVector(-500, -500, -200), FVector(500, 500, 300));
		UHBEncounterSubsystem* Subsystem = World->GetSubsystem<UHBEncounterSubsystem>();
		if (TestNotNull(TEXT("Actual encounter subsystem exists in the fixture"), Subsystem))
		{
			TestFalse(TEXT("Actual runtime still rejects blocked capsules"),
				Subsystem->StartEncounter(1, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Runtime failure records actual floor and blocking components before cleanup"),
				Error.Contains(TEXT("Cause=CapsuleOverlap")) && Error.Contains(TEXT("GroundActor=FixtureFloor"))
				&& Error.Contains(TEXT("BlockingActor=FixtureWall")) && Error.Contains(TEXT("BlockingComponent=")));
			TestTrue(TEXT("Runtime retains bool failure policy and reports query agreement"),
				Error.Contains(TEXT("FailureReason=MinCountNotMet")) && Error.Contains(TEXT("QueryMismatches=0")));
			Subsystem->ClearGeneration(1);
			Required->MaxCapsuleRadius = 1.f;
			TestFalse(TEXT("Capsule size failure remains a pre-search rejection"),
				Subsystem->StartEncounter(2, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Capsule size is typed without invented candidate failures"),
				Error.Contains(TEXT("CapsuleSizeExceeded:1")) && Error.Contains(TEXT("CandidatesChecked=0")));
			Required->MaxCapsuleRadius = 0.f;
			for (UBoxComponent* Box : {WallBox, WallSecondary, OtherWall, TouchBox})
				Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Missing navigation still rejects every otherwise clear candidate"),
				Subsystem->StartEncounter(3, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Actual monster navigation diagnostics retain agent, extent and exact prerequisite"),
				Error.Contains(TEXT("NavProjectionFailed:25")) && Error.Contains(TEXT("Condition=MissingSystem"))
				&& Error.Contains(TEXT("Stage=EnemyGround")) && Error.Contains(TEXT("AgentRadius="))
				&& Error.Contains(TEXT("Extent=")) && Error.Contains(TEXT("ProjectionQueried=0")));
			const AHBEnemySpawnPoint* PointDefaults = GetDefault<AHBEnemySpawnPoint>();
			TestTrue(TEXT("Existing enemy points default to legacy correction"), PointDefaults->bAllowPositionCorrection
				&& !PointDefaults->bUseCapsuleBasedCorrection && PointDefaults->CorrectionRadiusMultiplier == 2.f);
			Required->bUseCapsuleBasedCorrection = true;
			TestFalse(TEXT("Capsule correction still enforces real monster navigation"),
				Subsystem->StartEncounter(20, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			const float NativeRadius = NativeClass->GetDefaultObject<AEnemyCharacterBase>()->GetCapsuleComponent()->GetScaledCapsuleRadius();
			TestTrue(TEXT("Runtime uses the exact capsule radius from the selected class"),
				Error.Contains(FString::Printf(TEXT("CorrectionBasis=EnemyCapsule ShapeRadius=%.3f"), NativeRadius))
				&& Error.Contains(FString::Printf(TEXT("SearchRadius=%.3f CandidateBudget=33"), NativeRadius * 2.))
				&& Error.Contains(TEXT("NavProjectionFailed:33")));
			Group.Monsters = {{NativeClass, 1}, {BladeClass, 1}};
			Setup->Encounter->Groups = {Group};
			TestFalse(TEXT("Each rejected enemy class gets its own capsule-sized search"),
				Subsystem->StartEncounter(21, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			for (UClass* EnemyClass : {NativeClass, BladeClass})
			{
				const int32 ClassStart = Error.Find(TEXT("Class=") + EnemyClass->GetPathName());
				if (TestTrue(TEXT("Each enemy class retains a distinct diagnostic section"), ClassStart != INDEX_NONE))
				{
					const int32 NextClass = Error.Find(TEXT("; Group="), ESearchCase::CaseSensitive, ESearchDir::FromStart, ClassStart);
					const FString Section = Error.Mid(ClassStart, NextClass == INDEX_NONE ? MAX_int32 : NextClass - ClassStart);
					const float ClassRadius = EnemyClass->GetDefaultObject<AEnemyCharacterBase>()->GetCapsuleComponent()->GetScaledCapsuleRadius();
					TestTrue(TEXT("Enemy classes cannot borrow another class's shape radius"),
						Section.Contains(FString::Printf(TEXT("CorrectionBasis=EnemyCapsule ShapeRadius=%.3f"), ClassRadius))
						&& Section.Contains(TEXT("CandidateBudget=33")));
				}
			}
			Group.Monsters = {{NativeClass, 1}};
			Setup->Encounter->Groups = {Group};
			Required->bAllowPositionCorrection = false;
			TestFalse(TEXT("Disabled enemy correction does not bypass original navigation checks"),
				Subsystem->StartEncounter(22, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Disabled enemy correction performs one original candidate"),
				Error.Contains(TEXT("NavProjectionFailed:1")) && Error.Contains(TEXT("SearchRadius=0.000 CandidateBudget=1")));
			Required->bAllowPositionCorrection = true;
			Required->CorrectionRadiusMultiplier = 0.f;
			TestFalse(TEXT("Zero enemy multiplier still runs original safety checks"),
				Subsystem->StartEncounter(23, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Zero enemy multiplier performs one original candidate"), Error.Contains(TEXT("NavProjectionFailed:1")));
			Required->CorrectionRadiusMultiplier = -1.f;
			TestFalse(TEXT("Invalid enemy correction multiplier fails before candidate queries"),
				Subsystem->StartEncounter(24, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Invalid enemy correction is reported as configuration failure"),
				Error.Contains(TEXT("InvalidConfiguration:1")) && Error.Contains(TEXT("CandidatesChecked=0")));
			Required->bUseCapsuleBasedCorrection = false;
			Required->CorrectionRadiusMultiplier = 2.f;
			TestFalse(TEXT("Actual monster entry spacing takes priority over navigation"),
				Subsystem->StartEncounter(4, 836511139, 11, World->PersistentLevel, Bounds, {FVector::ZeroVector}, Error, true));
			TestTrue(TEXT("Entry spacing has its own candidate counts"),
				Error.Contains(TEXT("EntrySpacing:25")) && !Error.Contains(TEXT("Condition=MissingSystem")));
			auto* Exclusion = World->SpawnActor<AHBRoomSpawnExclusionVolume>();
			Exclusion->Bounds->SetBoxExtent(FVector(100, 100, 200));
			TestFalse(TEXT("Actual authored exclusion still rejects monster candidates"),
				Subsystem->StartEncounter(5, 836511139, 11, World->PersistentLevel, Bounds, {FVector(400, 400, 0)}, Error, true));
			TestTrue(TEXT("Exclusion has its own counts"), Error.Contains(TEXT("ExcludedArea:25")));
		}
	}
	const FString SavedBlockerPath = WallBox->GetOwner()->GetPathName();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	TestTrue(TEXT("Diagnostic strings survive world and actor cleanup"), Failure.Format().Contains(SavedBlockerPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterFailureDiagnosticsTest,
	"HeavenBreach.PCGModule.Encounter.FailureDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterFailureDiagnosticsTest::RunTest(const FString&)
{
	UClass* NativeClass = AEnemyCharacterBase::StaticClass();
	UClass* BladeClass = LoadClass<AEnemyCharacterBase>(nullptr,
		TEXT("/Game/Monsters/Remnant_Blade/Blueprint/BP_Remnant_Blade.BP_Remnant_Blade_C"));
	if (!TestNotNull(TEXT("Existing blade class is available for class attribution"), BladeClass)) return false;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<AHBEnemySpawnPoint*> Points;
	for (const TCHAR* Id : {TEXT("A_Required"), TEXT("B_Required"), TEXT("C_FirstFill"), TEXT("D_LastFill")})
	{
		auto* Point = World->SpawnActor<AHBEnemySpawnPoint>();
		Point->PointId = FName(Id);
		Point->GroupId = Points.Num() == 0 || Points.Num() == 2 ? TEXT("First") : TEXT("Last");
		Point->bRequired = Points.Num() < 2;
		if (!Points.IsEmpty()) Point->AllowedClasses.Add(NativeClass);
		Points.Add(Point);
	}
	UHBEncounterComponent* Config = NewObject<UHBEncounterComponent>(GetTransientPackage());
	FHBEncounterGroup First;
	First.GroupId = TEXT("First");
	First.MinCount = First.MaxCount = 1;
	First.Monsters = {{NativeClass, 1}, {BladeClass, 2}};
	FHBEncounterGroup Last = First;
	Last.GroupId = TEXT("Last");
	Config->Groups = {First, Last};
	TArray<FString> ConfigErrors;
	if (!TestTrue(TEXT("Diagnostic fixture has valid monster settings"), Config->Validate(ConfigErrors)))
	{
		AddError(FString::Join(ConfigErrors, TEXT("; ")));
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	TArray<FHBEncounterSpawnChoice> Placed;
	FString Error;
	FHBSpawnSelectionReport Report;
	int32 Calls = 0;
	bool bRejectB = true;
	auto RejectRequired = [&](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		++Calls;
		if (Choice.Point == Points[0])
		{
			Reason = Choice.EnemyClass.Get() == NativeClass ? TEXT("A-native-floor") : TEXT("A-blade-capsule");
			return false;
		}
		if (bRejectB && Choice.Point == Points[1]) { Reason = TEXT("B-unreachable-entry"); return false; }
		return true;
	};
	TestFalse(TEXT("A remains required after other points fill both group minima"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("Required failure is separate from minimum failure"),
		Report.FailureReason == EHBSpawnFailureReason::RequiredPointUnfilled && Report.FailedPointId == Points[0]->PointId);
	TestEqual(TEXT("Recording failures adds no placement attempts"), Calls, 5);
	TestEqual(TEXT("Other points satisfy both group minima"), Placed.Num(), 2);
	TestTrue(TEXT("A keeps both of its class failures"), Error.Contains(TEXT("A-native-floor")) && Error.Contains(TEXT("A-blade-capsule")));
	TestFalse(TEXT("A's final failure does not borrow B's reason"), Error.Contains(TEXT("B-unreachable-entry")));
	TestEqual(TEXT("A has one record for each attempted class"), Report.Points.FindChecked(Points[0]->PointId).ClassFailures.Num(), 2);
	TestTrue(TEXT("Class paths identify both failures"), Error.Contains(NativeClass->GetPathName()) && Error.Contains(BladeClass->GetPathName()));
	bRejectB = false;
	Calls = 0;
	TestFalse(TEXT("A's failure also survives B succeeding"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestEqual(TEXT("Report is reset on reuse"), Report.Points.FindChecked(Points[1]->PointId).ClassFailures.Num(), 0);
	TestEqual(TEXT("A retries each configured class only once"), Calls, 4);

	auto RejectLastGroup = [&](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.Point->GroupId == Last.GroupId) { Reason = TEXT("Last-group-spacing"); return false; }
		return RejectRequired(Choice, Reason);
	};
	TestFalse(TEXT("A later group's minimum failure keeps the original failure priority"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectLastGroup, Placed, Error, &Report));
	TestTrue(TEXT("Minimum failure names its group instead of a required point"),
		Report.FailureReason == EHBSpawnFailureReason::MinCountNotMet && Report.FailedGroupId == Last.GroupId && Report.FailedPointId.IsNone());
	TestTrue(TEXT("Minimum failure contains only this group's details"), Error.Contains(TEXT("Last-group-spacing")) && !Error.Contains(TEXT("A-native-floor")));
	TestTrue(TEXT("Minimum, target and placed counts are recorded"),
		Report.Groups.Last().MinCount == 1 && Report.Groups.Last().Target == 1 && Report.Groups.Last().Placed == 0);

	Config->Groups[0].Monsters = {{NativeClass, 1}};
	Points[0]->AllowedClasses = {BladeClass};
	Calls = 0;
	TestFalse(TEXT("A with no compatible class remains unfilled"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("No compatible class is reported without guessing a placement reason"), Error.Contains(TEXT("NoCompatibleClass")) && !Error.Contains(TEXT("A-native-floor")));
	TestEqual(TEXT("An incompatible point never invokes placement"), Calls, 2);
	TestTrue(TEXT("No class attempt is invented"), Report.Points.FindChecked(Points[0]->PointId).ClassFailures.IsEmpty());
	Points[0]->AllowedClasses.Reset();
	Points[0]->GroupId = TEXT("MissingGroup");
	TestFalse(TEXT("A required point with no matching group fails"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("Group filtering explains a point not attempted"), Error.Contains(TEXT("GroupMismatch")));
	Points[0]->GroupId = First.GroupId;
	Points[0]->bAllowCommon = false;
	TestFalse(TEXT("A required point with no allowed grade fails"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, RejectRequired, Placed, Error, &Report));
	TestTrue(TEXT("Grade filtering explains a point not attempted"), Error.Contains(TEXT("GradeMismatch")));
	Points[0]->bAllowCommon = true;
	Points[2]->bRequired = true;
	auto Accept = [](const FHBEncounterSpawnChoice&, FString&) { return true; };
	TestFalse(TEXT("Reaching target does not waive another required point"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 11, Accept, Placed, Error, &Report));
	TestTrue(TEXT("Target-skipped required point is identified"), Error.Contains(TEXT("TargetReached")));
	TestTrue(TEXT("Target-skipped point has no invented placement failure"), Report.Points.FindChecked(Report.FailedPointId).ClassFailures.IsEmpty());
	Points[2]->bRequired = false;
	Config->Groups[0] = First;
	auto Signature = [](const TArray<FHBEncounterSpawnChoice>& Choices)
	{
		FString Result;
		for (const auto& Choice : Choices) Result += Choice.PointId.ToString() + TEXT(":") + Choice.EnemyClass->GetPathName() + TEXT("|");
		return Result;
	};
	for (int32 Seed = 1; Seed <= 8; ++Seed)
	{
		TestTrue(TEXT("Legacy call without a report succeeds"), HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 11, Accept, Placed, Error));
		const FString Expected = Signature(Placed);
		TestTrue(TEXT("Optional report call succeeds"), HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 11, Accept, Placed, Error, &Report));
		TestEqual(TEXT("Optional report preserves class, point and quantity draws"), Signature(Placed), Expected);
		TestTrue(TEXT("Successful reuse clears the aggregate failure"), Report.FailureReason == EHBSpawnFailureReason::None && Report.FailedPointId.IsNone());
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
class FHBWaitForTestMGEncounter : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started = FPlatformTime::Seconds();
	bool bCreatedTestNavigationBounds = false;
public:
	explicit FHBWaitForTestMGEncounter(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
		const double Elapsed = FPlatformTime::Seconds() - Started;
		if (!World || World->GetTimeSeconds() < 1.f)
		{
			if (Elapsed < 60.) return false;
			Test->AddError(TEXT("TestMG PIE world did not start."));
			return true;
		}
		AHBDungeonModuleDefinition* Definition = nullptr;
		AHBEncounterSetupActor* Setup = nullptr;
		for (TActorIterator<AHBDungeonModuleDefinition> It(World); It; ++It) { Definition = *It; break; }
		Test->TestNotNull(TEXT("TestMG has a module definition"), Definition);
		if (!Definition) return true;
		for (AActor* Actor : Definition->GetLevel()->Actors)
			if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor)) { Setup = Candidate; break; }
		Test->TestNotNull(TEXT("TestMG has an Encounter Setup"), Setup);
		if (!Setup || !Setup->Encounter) return true;
		Test->TestTrue(TEXT("Standalone PIE owns the encounter on the server"), Setup->HasAuthority());
		UHBEncounterSubsystem* Subsystem = World->GetSubsystem<UHBEncounterSubsystem>();
		Test->TestNotNull(TEXT("TestMG world has an encounter subsystem"), Subsystem);
		if (!Subsystem) return true;
		TArray<FHBDungeonModuleIssue> Issues;
		if (!AHBDungeonModuleDefinition::ValidateLevel(Definition->GetLevel(), Issues))
		{
			for (const FHBDungeonModuleIssue& Issue : Issues)
				Test->AddError(FString::Printf(TEXT("TestMG authoring: %s: %s"), *Issue.Code.ToString(), *Issue.Message));
			return true;
		}
		FBox RoomBounds(ForceInit);
		TArray<FVector> Entries;
		for (AActor* Actor : Definition->GetLevel()->Actors)
		{
			if (!IsValid(Actor)) continue;
			TInlineComponentArray<UHBDungeonOccupancyComponent*> Occupancy(Actor);
			for (const auto* Box : Occupancy) RoomBounds += Box->Bounds.GetBox();
			TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(Actor);
			for (const auto* Connector : Connectors) Entries.Add(Connector->GetComponentLocation());
			if (const auto* Start = Cast<APlayerStart>(Actor)) Entries.Add(Start->GetActorLocation());
		}
		Test->TestTrue(TEXT("TestMG has room bounds"), RoomBounds.IsValid != 0);
		Test->TestTrue(TEXT("TestMG has an entry"), !Entries.IsEmpty());
		if (!RoomBounds.IsValid || Entries.IsEmpty()) return true;
		if (!bCreatedTestNavigationBounds)
		{
			int32 AuthoredVolumeCount = 0;
			for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) ++AuthoredVolumeCount;
			Test->TestEqual(TEXT("TestMG has no authored navigation volume"), AuthoredVolumeCount, 0);
			if (AuthoredVolumeCount != 0) return true;
			const FBox NavBounds = RoomBounds.ExpandBy(FVector(500, 500, 500));
			FActorSpawnParameters Params;
			Params.OverrideLevel = World->PersistentLevel;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(
				ANavMeshBoundsVolume::StaticClass(), NavBounds.GetCenter(), FRotator::ZeroRotator, Params);
			UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
			UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
			Test->TestTrue(TEXT("Test navigation volume and system exist"), Brush && Nav);
			if (!Brush || !Nav) return true;
			UBodySetup* Body = NewObject<UBodySetup>(Brush);
			FKBoxElem Box;
			Box.X = NavBounds.GetSize().X;
			Box.Y = NavBounds.GetSize().Y;
			Box.Z = NavBounds.GetSize().Z;
			Body->AggGeom.BoxElems.Add(Box);
			Brush->BrushBodySetup = Body;
			Brush->UpdateBounds();
			Nav->OnNavigationBoundsUpdated(Volume);
			bCreatedTestNavigationBounds = true;
			return false;
		}
		UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
		bool bHasNavigablePoint = false;
		FVector TestNavLocation = FVector::ZeroVector;
		for (AActor* Actor : Definition->GetLevel()->Actors)
			if (const AHBEnemySpawnPoint* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point))
			{
				FNavLocation OnNav;
				if (Nav && Nav->ProjectPointToNavigation(Point->GetActorLocation(), OnNav, FVector(150, 150, 350)))
				{ bHasNavigablePoint = true; TestNavLocation = OnNav.Location; break; }
			}
		if (!bHasNavigablePoint || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
		{
			if (Elapsed < 60.) return false;
			Test->AddError(TEXT("TestMG runtime navigation query did not become ready."));
			return true;
		}
		const ANavigationData* TestNavData = Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (Test->TestNotNull(TEXT("Actual NavMesh is available for projection diagnostics"), TestNavData))
		{
			const FNavAgentProperties* TestAgent = &TestNavData->GetConfig();
			const FVector Extent(150, 150, 350);
			FNavLocation Projected;
			FHBSpawnNavCheck Check;
			Test->TestTrue(TEXT("A valid actual navigation projection retains acceptance"),
				HBSpawnDiagnostics::ProjectNavigation(*World, Nav, TestAgent, TestNavData,
					TestNavLocation, Extent, -1., -1., nullptr, false, Projected, Check));
			Test->TestTrue(TEXT("Agent and projection extent are recorded from the existing query"),
				Check.AgentRadius == TestAgent->AgentRadius && Check.AgentHeight == TestAgent->AgentHeight
				&& Check.ProjectionExtent.Equals(Extent) && Check.bProjectionQueried && Check.bProjectionSucceeded);
			Test->TestFalse(TEXT("An actual projection exceeding height limits remains rejected"),
				HBSpawnDiagnostics::ProjectNavigation(*World, Nav, TestAgent, TestNavData,
					TestNavLocation + FVector(0, 0, 50), Extent, -1., 1., nullptr, false, Projected, Check));
			Test->TestTrue(TEXT("Height limit is distinguished from a failed projection query"),
				Check.Condition == EHBSpawnNavCondition::HeightExceeded && Check.bProjectionSucceeded);
			const FBox WrongBounds(TestNavLocation + FVector(5000, 5000, -100), TestNavLocation + FVector(5100, 5100, 100));
			Test->TestFalse(TEXT("An actual projection outside room bounds remains rejected"),
				HBSpawnDiagnostics::ProjectNavigation(*World, Nav, TestAgent, TestNavData,
					TestNavLocation, Extent, -1., -1., &WrongBounds, false, Projected, Check));
			Test->TestTrue(TEXT("Navigation room bounds have a distinct condition"), Check.Condition == EHBSpawnNavCondition::OutsideRoom);
			Test->TestFalse(TEXT("Missing agent does not execute a projection"),
				HBSpawnDiagnostics::ProjectNavigation(*World, Nav, nullptr, TestNavData,
					TestNavLocation, Extent, -1., -1., nullptr, false, Projected, Check));
			Test->TestTrue(TEXT("Reused navigation record clears earlier queried/succeeded flags"),
				Check.Condition == EHBSpawnNavCondition::MissingAgent && !Check.bProjectionQueried && !Check.bProjectionSucceeded);
			Test->TestFalse(TEXT("Missing agent navigation data does not execute a projection"),
				HBSpawnDiagnostics::ProjectNavigation(*World, Nav, TestAgent, nullptr,
					TestNavLocation, Extent, -1., -1., nullptr, false, Projected, Check));
			Test->TestTrue(TEXT("Missing NavData has its own condition"), Check.Condition == EHBSpawnNavCondition::MissingNavData);
			Test->TestFalse(TEXT("Failed path query cannot accept its partial path"), Check.SetPathResult(false, true, true));
			Test->TestTrue(TEXT("Failed query retains actual partial path information"),
				Check.Condition == EHBSpawnNavCondition::PathQueryFailed && Check.bPartialPath);
			Test->TestFalse(TEXT("Successful partial path remains disallowed"), Check.SetPathResult(true, true, true));
			Test->TestTrue(TEXT("Partial rejection is distinguished from query failure"), Check.Condition == EHBSpawnNavCondition::PartialPath);
		}
		FString Error;
		if (!Subsystem->StartEncounter(1, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error))
		{
			Test->AddError(FString::Printf(TEXT("TestMG real enemy spawn failed: %s"), *Error));
			return true;
		}
		Test->TestTrue(TEXT("Encounter prepared for this room"),
			Subsystem->IsPreparedFor(1, 7, Definition->GetLevel()));
		TArray<AEnemyCharacterBase*> Spawned;
		int32 CommonCount = 0, EliteCount = 0;
		AEnemyCharacterBase* CommonToKill = nullptr;
		for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
		{
			AEnemyCharacterBase* Enemy = *It;
			if (Enemy->GetLevel() != Definition->GetLevel()) continue;
			Spawned.Add(Enemy);
			Test->TestNotNull(TEXT("Spawned monster has an AI controller"), Enemy->GetController());
			if (AController* Controller = Enemy->GetController())
			{
				UStateTreeAIComponent* StateTree = Controller->FindComponentByClass<UStateTreeAIComponent>();
				Test->TestNotNull(TEXT("Spawned monster has a StateTree component"), StateTree);
				if (StateTree) Test->TestTrue(TEXT("Spawned monster StateTree runs after possession"), StateTree->IsRunning());
			}
			Test->TestNotNull(TEXT("Spawned monster has an ASC"), Enemy->GetEnemyASC());
			Test->TestFalse(TEXT("Spawned monster starts alive"), Enemy->IsDead());
			auto MatchesGrade = [Setup, Enemy](EHBEncounterGrade Grade)
			{
				return Setup->Encounter->Groups.ContainsByPredicate([Grade, Enemy](const FHBEncounterGroup& Group)
				{
					return Group.Grade == Grade && Group.Monsters.ContainsByPredicate(
						[Enemy](const FHBEncounterMonsterOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); });
				});
			};
			if (MatchesGrade(EHBEncounterGrade::Elite)) ++EliteCount;
			else if (MatchesGrade(EHBEncounterGrade::Common)) { ++CommonCount; CommonToKill = Enemy; }
			else Test->AddError(FString::Printf(TEXT("Unexpected monster class: %s"), *Enemy->GetClass()->GetName()));
		}
		Test->TestTrue(TEXT("One or two common monsters spawned"), CommonCount >= 1 && CommonCount <= 2);
		Test->TestEqual(TEXT("Stone Golem elite spawned once"), EliteCount, 1);
		Test->TestEqual(TEXT("Alive count matches real actors"),
			Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), Spawned.Num());
        Test->TestTrue(TEXT("Repeated encounter preparation does not spawn again"),
            Subsystem->StartEncounter(1, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error));
        Test->TestFalse(TEXT("Prepared encounter cannot change seed"),
            Subsystem->StartEncounter(1, 54321, 7, Definition->GetLevel(), RoomBounds, Entries, Error));
        Test->TestFalse(TEXT("Prepared encounter cannot change its room ID"),
            Subsystem->StartEncounter(1, 12345, 8, Definition->GetLevel(), RoomBounds, Entries, Error));
        Test->TestFalse(TEXT("New generation waits for explicit encounter cleanup"),
            Subsystem->StartEncounter(2, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error));
        Test->TestFalse(TEXT("Invalid encounter generation is rejected"),
            Subsystem->StartEncounter(0, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error));
        Test->TestFalse(TEXT("Invalid encounter room is rejected"),
            Subsystem->StartEncounter(1, 12345, INDEX_NONE, Definition->GetLevel(), RoomBounds, Entries, Error));
        int32 AfterRepeatedRequest = 0;
        for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
            if (It->GetLevel() == Definition->GetLevel()) ++AfterRepeatedRequest;
        Test->TestEqual(TEXT("Rejected encounter requests do not create extra actors"), AfterRepeatedRequest, Spawned.Num());
		if (CommonToKill)
		{
			const int32 BeforeDeath = Subsystem->GetAliveCount(1, 7, Definition->GetLevel());
			CommonToKill->BecomeDead();
			Test->TestTrue(TEXT("Real enemy death applied"), CommonToKill->IsDead());
			Test->TestEqual(TEXT("Death decrements alive count once"),
				Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), BeforeDeath - 1);
			CommonToKill->BecomeDead();
			Test->TestEqual(TEXT("Repeated death does not double count"),
				Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), BeforeDeath - 1);
		}
		Subsystem->ClearGeneration(1);
		Test->TestEqual(TEXT("Cleanup clears alive count"), Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), 0);
		Test->TestFalse(TEXT("Cleanup clears generation state"), Subsystem->IsPreparedFor(1, 7, Definition->GetLevel()));
		for (AEnemyCharacterBase* Enemy : Spawned)
			Test->TestTrue(TEXT("Cleanup destroys each spawned monster"), !IsValid(Enemy) || Enemy->IsActorBeingDestroyed());
		if (!Subsystem->StartEncounter(2, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error, true))
			Test->AddError(FString::Printf(TEXT("TestMG restart after cleanup failed: %s"), *Error));
		else
        {
            Subsystem->ClearGeneration(1);
            Test->TestTrue(TEXT("Late old-generation cleanup preserves the restarted encounter"),
                Subsystem->IsPreparedFor(2, 7, Definition->GetLevel()));
            Test->TestEqual(TEXT("Restart produces the same seeded count"),
                Subsystem->GetAliveCount(2, 7, Definition->GetLevel()), Spawned.Num());
            Test->TestFalse(TEXT("Deferred generation is prepared without activating AI"),
                Subsystem->IsActivatedFor(2, 7, Definition->GetLevel()));
            Test->TestTrue(TEXT("Repeated preparation reuses pending enemies without activating them"),
                Subsystem->StartEncounter(2, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error));
            Test->TestFalse(TEXT("Default immediate mode cannot override a prepared deferred encounter"),
                Subsystem->IsActivatedFor(2, 7, Definition->GetLevel()));
            Test->TestFalse(TEXT("Old generation cannot activate new room actors"), Subsystem->ActivateGeneration(1, Error));
            for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
                if (It->GetLevel() == Definition->GetLevel() && !It->IsActorBeingDestroyed())
                    Test->TestNull(TEXT("Deferred enemies have no controller before activation"), It->GetController());
            Test->TestTrue(TEXT("Current prepared generation can activate"), Subsystem->ActivateGeneration(2, Error));
            Test->TestTrue(TEXT("Current encounter is active after possession"),
                Subsystem->IsActivatedFor(2, 7, Definition->GetLevel()));
            Test->TestTrue(TEXT("Repeated generation activation succeeds without restarting AI"), Subsystem->ActivateGeneration(2, Error));
            for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
                if (It->GetLevel() == Definition->GetLevel() && !It->IsActorBeingDestroyed())
                    Test->TestNotNull(TEXT("Activated deferred enemies have controllers"), It->GetController());
        }
		Subsystem->ClearGeneration(2);

		const TArray<FHBEncounterGroup> SavedGroups = Setup->Encounter->Groups;
		const TArray<FVector> UnreachableEntries{RoomBounds.Max + FVector(100000, 100000, 100000)};
		for (FHBEncounterGroup& Group : Setup->Encounter->Groups) Group.bRequireEntryPath = true;
		Test->TestFalse(TEXT("Entry path requirement rejects an unreachable entry"),
			Subsystem->StartEncounter(3, 12345, 7, Definition->GetLevel(), RoomBounds, UnreachableEntries, Error, true));
		Test->TestTrue(TEXT("Unreachable entry reports the navigation path reason"),
			Error.Contains(TEXT("no complete navigation path")));
		Test->TestTrue(TEXT("Actual entry path failure keeps its typed cause and failed entry projection"),
			Error.Contains(TEXT("Cause=NoCompleteEntryPath")) && Error.Contains(TEXT("Stage=EntryPath"))
			&& Error.Contains(TEXT("Condition=ProjectionQueryFailed"))
			&& Error.Contains(TEXT("EntriesAvailable=1 EntriesChecked=1 Direction=PlacementToEntry")));
		Subsystem->ClearGeneration(3);
		for (FHBEncounterGroup& Group : Setup->Encounter->Groups) Group.bRequireEntryPath = false;
		if (Subsystem->StartEncounter(4, 12345, 7, Definition->GetLevel(), RoomBounds, UnreachableEntries, Error, true))
		{
			Test->TestTrue(TEXT("Local navigation permits spawning without an entry path"),
				Subsystem->IsPreparedFor(4, 7, Definition->GetLevel()));
			Test->TestEqual(TEXT("Skipping entry paths preserves the seeded spawn count"),
				Subsystem->GetAliveCount(4, 7, Definition->GetLevel()), Spawned.Num());
		}
		else Test->AddError(FString::Printf(TEXT("Spawn without entry path failed: %s"), *Error));
		Subsystem->ClearGeneration(4);

		TArray<TPair<AHBEnemySpawnPoint*, FVector>> SavedPoints;
		for (AActor* Actor : Definition->GetLevel()->Actors)
			if (auto* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point))
			{
				SavedPoints.Emplace(Point, Point->GetActorLocation());
				Point->SetActorLocation(Point->GetActorLocation() + FVector(0, 0, 100000));
			}
		Test->TestFalse(TEXT("Disabling entry paths still rejects points without a safe floor"),
			Subsystem->StartEncounter(5, 12345, 7, Definition->GetLevel(), RoomBounds, UnreachableEntries, Error, true));
		Test->TestTrue(TEXT("Floor validation remains active"), Error.Contains(TEXT("No safe floor")));
		Subsystem->ClearGeneration(5);
		for (const auto& Saved : SavedPoints) Saved.Key->SetActorLocation(Saved.Value);
		Setup->Encounter->Groups = SavedGroups;
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGEncounterIntegrationTest, "HeavenBreach.PCGModule.Encounter.TestMGStandalone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGEncounterIntegrationTest::RunTest(const FString&)
{
	const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestMG");
	if (!FPackageName::DoesPackageExist(Map)) { AddError(TEXT("TestMG fixture map is missing.")); return false; }
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	const TSharedRef<TSubclassOf<AGameModeBase>> PreviousMapMode = MakeShared<TSubclassOf<AGameModeBase>>();
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, PreviousMapMode]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				AWorldSettings* Settings = Context.World()->GetWorldSettings();
				*PreviousMapMode = Settings->DefaultGameMode;
				Settings->DefaultGameMode = AGameModeBase::StaticClass();
				return true;
			}
		AddError(TEXT("Could not isolate TestMG from the project's gameplay GameMode."));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGEncounter(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients, PreviousMapMode]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Editor && Context.World())
				Context.World()->GetWorldSettings()->DefaultGameMode = *PreviousMapMode;
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}

// Shared only by the enemy/chest automation translation units; no runtime API changes.
namespace HBEntryPathTestFixture
{
    // Locations: marker, valid entry, disconnected entry, outgoing-only entry,
    // unprojectable entry, and a physical floor outside the navigation bounds.
    using FVerify = TFunction<void(UWorld&, const FBox&, const TArray<FVector>&)>;

    UBoxComponent* AddCollisionBlocker(UWorld& World, const FVector& Marker)
    {
        AActor* Actor = World.SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(5, 200, 150));
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetCanEverAffectNavigation(false);
        Box->RegisterComponent();
        // Leave the marker's vertical floor trace clear, but intersect the spawned body.
        Box->SetWorldLocation(Marker + FVector(6, 0, 149.6));
        return Box;
    }

    class FWaitForNavigation : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        FVerify Verify;
        double Started = 0.;
        bool bCreated = false;
        FBox RoomBounds = FBox(ForceInit);
        TArray<FVector> Locations;
    public:
        FWaitForNavigation(FAutomationTestBase* InTest, FVerify InVerify)
            : Test(InTest), Verify(MoveTemp(InVerify)) {}
        virtual bool Update() override
        {
            if (Started == 0.) Started = FPlatformTime::Seconds();
            UWorld* World = nullptr;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
            const bool bTimedOut = FPlatformTime::Seconds() - Started > 90.;
            if (!World || World->GetTimeSeconds() < 0.5f)
            {
                if (!bTimedOut) return false;
                Test->AddError(TEXT("Entry path fixture PIE did not start.")); return true;
            }
            auto* Nav = UNavigationSystemV1::GetCurrent(World);
            if (!Test->TestNotNull(TEXT("Entry path fixture has a real navigation system"), Nav)) return true;
            if (!bCreated)
            {
                const FVector Origin(200000, 0, 10000);
                Locations = {Origin + FVector(-400, 0, 0.4), Origin + FVector(-400, 600, 0),
                    Origin + FVector(0, 5000, 0), Origin + FVector(1600, 0, 0),
                    Origin + FVector(50000, 50000, 50000), Origin + FVector(0, 15000, 0.4)};
                UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
                if (!Test->TestNotNull(TEXT("Fixture cube supplies physical and navigation geometry"), Cube)) return true;
                auto AddFloor = [&](const FVector& Center, const FVector& Extent)
                {
                    AActor* Actor = World->SpawnActor<AActor>();
                    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
                    Actor->SetRootComponent(Mesh);
                    Mesh->SetMobility(EComponentMobility::Static);
                    Mesh->SetStaticMesh(Cube);
                    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
                    Mesh->SetWorldLocation(Center - FVector(0, 0, Extent.Z));
                    Mesh->SetWorldScale3D(Extent / 50.);
                    Mesh->RegisterComponent();
                    RoomBounds += FBox(Center - FVector(Extent.X, Extent.Y, Extent.Z * 2.),
                        Center + FVector(Extent.X, Extent.Y, 0));
                };
                AddFloor(Origin, FVector(1000, 1000, 20));
                AddFloor(Locations[2], FVector(600, 600, 20));
                AddFloor(Locations[3], FVector(400, 600, 20));
                const FBox NavBounds = RoomBounds.ExpandBy(FVector(300, 300, 500));
                AddFloor(Locations[5] - FVector(0, 0, 0.4), FVector(600, 600, 20));
                RoomBounds = RoomBounds.ExpandBy(FVector(0, 0, 500));

                const FTransform LinkTransform(Origin);
                auto* Link = World->SpawnActorDeferred<ANavLinkProxy>(ANavLinkProxy::StaticClass(),
                    LinkTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (!Test->TestNotNull(TEXT("One-way link fixture is created"), Link)) return true;
                FNavigationLink OneWay;
                OneWay.Left = FVector(800, 0, 10);
                OneWay.Right = FVector(1400, 0, 10);
                OneWay.Direction = ENavLinkDirection::LeftToRight;
                Link->PointLinks = {OneWay};
                Link->bSmartLinkIsRelevant = false;
                Link->FinishSpawning(LinkTransform);
                UNavigationSystemV1::UpdateActorInNavOctree(*Link);

                auto* Volume = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(),
                    NavBounds.GetCenter(), FRotator::ZeroRotator);
                UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
                if (!Test->TestNotNull(TEXT("Temporary navigation volume is created"), Brush)) return true;
                UBodySetup* Body = NewObject<UBodySetup>(Brush);
                FKBoxElem Box;
                Box.X = NavBounds.GetSize().X; Box.Y = NavBounds.GetSize().Y; Box.Z = NavBounds.GetSize().Z;
                Body->AggGeom.BoxElems.Add(Box);
                Brush->BrushBodySetup = Body;
                Brush->UpdateBounds();
                Nav->OnNavigationBoundsUpdated(Volume);
                if (auto* Mode = World->GetAuthGameMode()) Mode->DefaultPawnClass = ACharacter::StaticClass();
                bCreated = true;
                return false;
            }
            const ANavigationData* Data = Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
            FNavLocation Projected[4];
            bool bReady = Data && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
            for (int32 Index = 0; bReady && Index < 4; ++Index)
                bReady = Nav->ProjectPointToNavigation(Locations[Index], Projected[Index], FVector(100, 100, 200), Data);
            if (!bReady)
            {
                if (!bTimedOut) return false;
                Test->AddError(TEXT("Entry path fixture navigation did not become ready.")); return true;
            }
            const auto& Agent = Data->GetConfig();
            auto Path = [&](int32 From, int32 To)
            {
                FPathFindingQuery Query(Nav, *Data, Projected[From].Location, Projected[To].Location);
                Query.SetAllowPartialPaths(false).SetNavAgentProperties(Agent);
                return Nav->FindPathSync(Agent, Query);
            };
            const auto Forward = Path(0, 3), Reverse = Path(3, 0), Disconnected = Path(0, 2);
            const bool bFixtureValid = Forward.IsSuccessful() && Forward.Path.IsValid() && !Forward.Path->IsPartial()
                && !Reverse.IsSuccessful() && !Disconnected.IsSuccessful()
                && Disconnected.Path.IsValid() && Disconnected.Path->IsPartial();
            Test->TestTrue(TEXT("Real NavMesh has an outgoing-only link and a disconnected partial path"), bFixtureValid);
            FNavLocation InvalidProjection;
            Test->TestFalse(TEXT("Invalid first entry actually fails projection"),
                Nav->ProjectPointToNavigation(Locations[4], InvalidProjection, FVector(100, 100, 200), Data));
            Test->TestFalse(TEXT("Physical safety floor actually has no local navigation"),
                Nav->ProjectPointToNavigation(Locations[5], InvalidProjection, FVector(100, 100, 200), Data));
            if (bFixtureValid) Verify(*World, RoomBounds, Locations);
            return true;
        }
    };

    void Queue(FAutomationTestBase* Test, FVerify Verify)
    {
        EPlayNetMode PreviousMode;
        int32 PreviousClients;
        GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
        GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
        const auto PreviousMapMode = MakeShared<TSubclassOf<AGameModeBase>>();
        const auto PreviousDirty = MakeShared<bool>(false);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
        Test->AddCommand(new FEditorLoadMap(TEXT("/Engine/Maps/Entry")));
        Test->AddCommand(new FFunctionLatentCommand([Test, PreviousMapMode, PreviousDirty]()
        {
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::Editor && Context.World())
                {
                    auto* World = Context.World();
                    *PreviousMapMode = World->GetWorldSettings()->DefaultGameMode;
                    *PreviousDirty = World->GetOutermost()->IsDirty();
                    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
                    return true;
                }
            Test->AddError(TEXT("Entry path fixture has no editor world.")); return true;
        }));
        Test->AddCommand(new FStartPIECommand(false));
        Test->AddCommand(new FWaitForNavigation(Test, MoveTemp(Verify)));
        Test->AddCommand(new FEndPlayMapCommand());
        Test->AddCommand(new FFunctionLatentCommand([Test, PreviousMode, PreviousClients, PreviousMapMode, PreviousDirty]()
        {
            for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return false;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::Editor && Context.World())
                {
                    Context.World()->GetWorldSettings()->DefaultGameMode = *PreviousMapMode;
                    Test->TestEqual(TEXT("Fixture did not change the original map's dirty state"),
                        Context.World()->GetOutermost()->IsDirty(), *PreviousDirty);
                }
            GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
            GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
            return true;
        }));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterEntryPathPolicyTest, "HeavenBreach.PCGModule.Encounter.EntryPathPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterEntryPathPolicyTest::RunTest(const FString&)
{
    HBEntryPathTestFixture::Queue(this, [this](UWorld& World, const FBox& Bounds, const TArray<FVector>& Locations)
    {
        auto* Setup = World.SpawnActor<AHBEncounterSetupActor>();
        auto* Point = World.SpawnActor<AHBEnemySpawnPoint>();
        Point->PointId = TEXT("EntryPolicy");
        Point->GroupId = TEXT("Field");
        Point->bRequired = true;
        Point->bAllowPositionCorrection = false;
        Point->SetActorLocation(Locations[0]);
        FHBEncounterGroup Group;
        Group.GroupId = Point->GroupId;
        Group.MinCount = Group.MaxCount = 1;
        Group.Monsters = {{AEnemyCharacterBase::StaticClass(), 1}};
        Setup->Encounter->Groups = {Group};
        auto* Subsystem = World.GetSubsystem<UHBEncounterSubsystem>();
        if (!TestNotNull(TEXT("Enemy entry policy uses the actual encounter subsystem"), Subsystem)) return;
        FString Error;
        int32 Generation = 100;
        auto Run = [&](const TCHAR* Label, const TArray<FVector>& Entries, bool bExpected, const TCHAR* Cause = nullptr)
        {
            const bool bResult = Subsystem->StartEncounter(Generation, 12345, 0, World.PersistentLevel, Bounds, Entries, Error, true);
            TestEqual(Label, bResult, bExpected);
            if (bResult) TestEqual(TEXT("Successful entry policy case spawns exactly one enemy"),
                Subsystem->GetAliveCount(Generation, 0, World.PersistentLevel), 1);
            if (Cause) TestTrue(FString(Label) + TEXT(" retains its exact cause"), Error.Contains(Cause));
            AddInfo(FString::Printf(TEXT("%s: Spawned=%d Expected=%d%s"), Label, bResult, bExpected,
                bResult ? TEXT("") : *(TEXT(" Diagnostic=") + Error)));
            Subsystem->ClearGeneration(Generation++);
        };
        Run(TEXT("Enemy tries the valid second entry after a failed projection"), {Locations[4], Locations[1]}, true);
        Run(TEXT("Enemy tries the valid second entry after a partial path"), {Locations[2], Locations[1]}, true);
        Run(TEXT("Enemy rejects all invalid active entries"), {Locations[4], Locations[2]}, false, TEXT("Cause=NoCompleteEntryPath"));
        TestTrue(TEXT("Enemy checks both entries in placement-to-entry direction"),
            Error.Contains(TEXT("EntriesAvailable=2 EntriesChecked=2 Direction=PlacementToEntry"))
            && Error.Contains(TEXT("EntryIndex=0")) && Error.Contains(TEXT("Condition=ProjectionQueryFailed"))
            && Error.Contains(TEXT("EntryIndex=1")) && Error.Contains(TEXT("PathQueried=1")) && Error.Contains(TEXT("PartialPath=1")));
        Run(TEXT("Enemy uses the outgoing-only link from placement to entry"), {Locations[3]}, true);
        Setup->Encounter->Groups[0].bRequireEntryPath = false;
        Run(TEXT("Enemy may skip entry paths while keeping valid local navigation"), {Locations[4], Locations[2]}, true);
        Point->SetActorLocation(Locations[5]);
        Run(TEXT("Skipping enemy entry paths still requires local navigation"), {Locations[4]}, false, TEXT("Cause=NavProjectionFailed"));
        Point->SetActorLocation(Locations[0]);
        auto* Blocker = HBEntryPathTestFixture::AddCollisionBlocker(World, Locations[0]);
        Run(TEXT("Skipping enemy entry paths still rejects blocking capsule geometry"), {Locations[4]}, false, TEXT("Cause=CapsuleOverlap"));
        Blocker->GetOwner()->Destroy();
        Point->Destroy(); Setup->Destroy();
    });
    return true;
}
#endif
