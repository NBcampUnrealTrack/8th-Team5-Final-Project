#include "PCGModule/HBSpawnGroundQuery.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "PCGModule/HBSpawnDiagnostics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FGroundQueryWorld
	{
		UWorld* World = nullptr;
		FGroundQueryWorld()
		{
			const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FGroundQueryWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		UBoxComponent* AddBox(const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCanEverAffectNavigation(false);
			Box->RegisterComponent();
			Actor->SetActorLocationAndRotation(Center, Rotation);
			return Box;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnGroundSeamTest, "HeavenBreach.PCGModule.SpawnGround.Seams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnGroundSeamTest::RunTest(const FString&)
{
	FGroundQueryWorld TestWorld;
	const FBox Bounds(FVector(-500., -500., -100.), FVector(500., 500., 300.));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBGroundSeamTest), false);
	// Four tiles meet around a real 2 cm gap, reproducing a marker on a tile corner.
	for (double X : {-101., 101.})
		for (double Y : {-101., 101.})
			TestWorld.AddBox(FVector(X, Y, -10.), FVector(100., 100., 10.));
	const FVector Marker(0., 0., 0.4);
	FHitResult Original;
	TestFalse(TEXT("The original single floor trace misses the tile corner gap"),
		TestWorld.World->LineTraceSingleByChannel(Original, Marker + FVector(0., 0., 100.),
			Marker - FVector(0., 0., 200.), ECC_WorldStatic, Query));
	FHitResult Floor;
	FString Error;
	auto Accept = [](const FHitResult&, FString&) { return true; };
	TestTrue(TEXT("Nearby physical floor recovers a tile corner"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
	TestTrue(TEXT("Recovered point remains within 30 cm"),
		FVector::Dist2D(Marker, Floor.ImpactPoint) <= HBSpawnGroundQuery::MaxHorizontalAdjustment + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Recovered point has a flat physical floor"), Floor.ImpactNormal.Z >= 0.7 && FMath::Abs(Floor.ImpactPoint.Z) < 0.01);
	const FVector FirstGround = Floor.ImpactPoint;
	for (int32 Replay = 0; Replay < 3; ++Replay)
	{
		TestTrue(TEXT("Recovery replays without consuming random draws"),
			HBSpawnGroundQuery::FindPlacement(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
		TestTrue(TEXT("Recovery picks the same position"), Floor.ImpactPoint.Equals(FirstGround));
	}
	const FVector ValidMarker(100., 100., 0.4);
	int32 Checks = 0;
	TestTrue(TEXT("A valid authored location is preserved"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, ValidMarker, FQuat::Identity, Bounds, 0.7f, Query,
			[&](const FHitResult&, FString&) { ++Checks; return true; }, Floor, Error));
	TestTrue(TEXT("No horizontal adjustment for a valid original marker"), FVector::Dist2D(ValidMarker, Floor.ImpactPoint) < 0.01);
	TestEqual(TEXT("Original valid point stops the search immediately"), Checks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnGroundSafetyTest, "HeavenBreach.PCGModule.SpawnGround.Safety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnGroundSafetyTest::RunTest(const FString&)
{
	FGroundQueryWorld TestWorld;
	const FBox Bounds(FVector(-500., -500., -100.), FVector(500., 500., 300.));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBGroundSafetyTest), false);
	FHitResult Floor;
	FString Error;
	int32 Checks = 0;
	auto Accept = [&](const FHitResult&, FString&) { ++Checks; return true; };
	TestFalse(TEXT("An actual empty space cannot produce a floor"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 0.4), FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
	TestTrue(TEXT("Missing floor has a distinct diagnostic"), Error.Contains(TEXT("no floor=25")));
	TestEqual(TEXT("Placement is never attempted without a physical floor"), Checks, 0);
	UBoxComponent* Box = TestWorld.AddBox(FVector(100., 0., -10.), FVector(10., 100., 10.));
	TestFalse(TEXT("A floor 90 cm away cannot be used"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 0.4), FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
	Box->SetBoxExtent(FVector(200., 200., 10.));
	Box->GetOwner()->SetActorLocation(FVector(0., 0., -10.));
	TestFalse(TEXT("A marker far above the floor retains the vertical limit"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 1000.), FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
	TestEqual(TEXT("Far below marker floors never reach placement validation"), Checks, 0);
	const FBox WrongFloorBounds(FVector(-500., -500., 10.), FVector(500., 500., 300.));
	TestFalse(TEXT("Physical floors outside the source room bounds stay rejected"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 20.), FQuat::Identity, WrongFloorBounds, 0.7f, Query, Accept, Floor, Error));
	TestTrue(TEXT("Room bounds rejection is diagnosed"), Error.Contains(TEXT("outside room=25")));
	UBoxComponent* Obstacle = TestWorld.AddBox(FVector(0., 0., 30.), FVector(3., 3., 30.));
	Query.AddIgnoredActor(Obstacle->GetOwner());
	int32 Blocked = 0;
	TestTrue(TEXT("A blocked original position can recover only after collision validation"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 0.4), FQuat::Identity, Bounds, 0.7f, Query,
			[&](const FHitResult& Candidate, FString& Reason)
			{
				FCollisionQueryParams OverlapQuery(SCENE_QUERY_STAT(HBGroundOverlapTest), false);
				if (TestWorld.World->OverlapBlockingTestByProfile(Candidate.ImpactPoint + FVector(0., 0., 12.), FQuat::Identity,
					TEXT("BlockAll"), FCollisionShape::MakeCapsule(2.f, 10.f), OverlapQuery))
				{
					++Blocked; Reason = TEXT("Capsule overlaps blocking geometry."); return false;
				}
				return true;
			}, Floor, Error));
	TestTrue(TEXT("The blocking original candidate was rejected"), Blocked > 0);
	TestTrue(TEXT("Accepted correction clears the obstacle"), FVector::Dist2D(FVector::ZeroVector, Floor.ImpactPoint) >= 10. - KINDA_SMALL_NUMBER);
	Checks = 0;
	TestFalse(TEXT("Every candidate must still pass navigation and other caller safety checks"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 0.4), FQuat::Identity, Bounds, 0.7f, Query,
			[&](const FHitResult&, FString& Reason) { ++Checks; Reason = TEXT("No nearby navigation surface for this agent."); return false; }, Floor, Error));
	TestEqual(TEXT("No candidate bypasses caller validation"), Checks, 25);
	TestTrue(TEXT("Caller rejection reasons appear in the final diagnostic"), Error.Contains(TEXT("navigation surface")) && Error.Contains(TEXT("safety rejected=25")));
	Obstacle->GetOwner()->Destroy();
	Box->GetOwner()->SetActorLocationAndRotation(FVector::ZeroVector, FRotator(60., 0., 0.));
	Checks = 0;
	TestFalse(TEXT("An actual steep floor remains rejected"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, FVector(0., 0., 0.4), FQuat::Identity, Bounds, 0.7f, Query, Accept, Floor, Error));
	TestTrue(TEXT("Slope rejection includes the hit normal and actor"), Error.Contains(TEXT("normal=")) && Error.Contains(TEXT("actor=")));
	TestEqual(TEXT("Steep floors never reach caller placement checks"), Checks, 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnGroundFailureDiagnosticsTest,
	"HeavenBreach.PCGModule.SpawnGround.FailureDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnGroundFailureDiagnosticsTest::RunTest(const FString&)
{
	FGroundQueryWorld TestWorld;
	const FVector Marker(0, 0, 0.4);
	const FBox Bounds(FVector(-500, -500, -200), FVector(500, 500, 300));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HBGroundFailureDiagnostics), false);
	FHitResult Floor;
	FString Error;
	FHBSpawnPlacementReport Report;
	int32 Calls = 0;
	auto Accept = [&](const FHitResult&, FString&, FHBSpawnCandidateReport&) { ++Calls; return true; };
	TestFalse(TEXT("Empty physics world rejects all candidates with typed NoFloor"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f,
			Query, Accept, Floor, Error, Report));
	TestEqual(TEXT("No floor count is exact"), Report.CountFailureReasons().FindRef(EHBSpawnFailureReason::NoFloor), 25);
	TestEqual(TEXT("No floor never calls placement"), Calls, 0);
	TestEqual(TEXT("All tested candidates are retained"), Report.Candidates.Num(), 25);
	for (int32 Index = 0; Index < Report.Candidates.Num(); ++Index)
	{
		const auto& Candidate = Report.Candidates[Index];
		TestEqual(TEXT("Full candidate indices are contiguous"), Candidate.CandidateIndex, Index);
		TestFalse(TEXT("Missing floor is not represented by an invented hit"), Candidate.bHasFloor);
	}
	TestTrue(TEXT("Original marker and first radial direction retain their order"),
		Report.Candidates[0].CandidateLocation.Equals(Marker)
		&& Report.Candidates[1].CandidateLocation.Equals(Marker + FVector(10, 0, 0))
		&& Report.Candidates[9].CandidateLocation.Equals(Marker + FVector(20, 0, 0))
		&& Report.Candidates[17].CandidateLocation.Equals(Marker + FVector(30, 0, 0)));

	// Exactly nine authored candidates hit this strip; the other sixteen have no physical floor.
	UBoxComponent* Strip = TestWorld.AddBox(FVector(20, 0, -10), FVector(15, 500, 10));
	TArray<FVector> TypedChecks, LegacyChecks;
	auto RejectMixed = [&](const FHitResult& Hit, FString& Reason, FHBSpawnCandidateReport& Candidate)
	{
		TypedChecks.Add(Hit.ImpactPoint);
		Candidate.Cause = Candidate.CandidateIndex == 1 ? EHBSpawnFailureReason::ExcludedArea
			: Candidate.CandidateIndex == 9 ? EHBSpawnFailureReason::EntrySpacing : EHBSpawnFailureReason::NavProjectionFailed;
		Reason = TEXT("Mixed placement fixture");
		return false;
	};
	TestFalse(TEXT("Ground and placement failures can coexist without borrowing the last cause"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f,
			Query, RejectMixed, Floor, Error, Report));
	const auto Counts = Report.CountFailureReasons();
	TestEqual(TEXT("Mixed missing floor count"), Counts.FindRef(EHBSpawnFailureReason::NoFloor), 16);
	TestEqual(TEXT("Mixed exclusion count"), Counts.FindRef(EHBSpawnFailureReason::ExcludedArea), 1);
	TestEqual(TEXT("Mixed entry distance count"), Counts.FindRef(EHBSpawnFailureReason::EntrySpacing), 1);
	TestEqual(TEXT("Mixed navigation count"), Counts.FindRef(EHBSpawnFailureReason::NavProjectionFailed), 7);
	TestEqual(TEXT("Aggregate rejection is not counted as another candidate"), Counts.FindRef(EHBSpawnFailureReason::PlacementRejected), 0);
	TestTrue(TEXT("Original and intermediate causes survive the final failure"),
		Report.Candidates[0].Cause == EHBSpawnFailureReason::NoFloor
		&& Report.Candidates[1].Cause == EHBSpawnFailureReason::ExcludedArea
		&& Report.Candidates[9].Cause == EHBSpawnFailureReason::EntrySpacing);
	const FString TypedError = Error;
	TestFalse(TEXT("Legacy callback retains the same rejection behavior"),
		HBSpawnGroundQuery::FindPlacement(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f, Query,
			[&](const FHitResult& Hit, FString& Reason)
			{ LegacyChecks.Add(Hit.ImpactPoint); Reason = TEXT("Mixed placement fixture"); return false; }, Floor, Error));
	TestEqual(TEXT("Legacy error text is preserved"), Error, TypedError);
	TestEqual(TEXT("Typed diagnostics do not add safety checks"), TypedChecks.Num(), 9);
	TestTrue(TEXT("Typed and legacy callback positions and order match"), TypedChecks == LegacyChecks);

	Strip->SetBoxExtent(FVector(500, 500, 10));
	Strip->GetOwner()->SetActorLocation(FVector(0, 0, -10));
	const FBox WrongFloorBounds(FVector(-500, -500, 10), FVector(500, 500, 300));
	TestFalse(TEXT("Floor outside the room has a typed reason"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, FVector(0, 0, 20), FQuat::Identity,
			WrongFloorBounds, 0.7f, Query, Accept, Floor, Error, Report));
	TestEqual(TEXT("Floor bounds count"), Report.CountFailureReasons().FindRef(EHBSpawnFailureReason::OutsideRoom), 25);
	TestTrue(TEXT("Rejected physical floor metadata is retained"), Report.Candidates[0].bHasFloor
		&& Report.Candidates[0].GroundActorPath == Strip->GetOwner()->GetPathName());
	const FBox NarrowBounds(FVector(-4, -4, -200), FVector(4, 4, 300));
	TestFalse(TEXT("Horizontal bounds and caller rejections remain distinct"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, NarrowBounds, 0.7f, Query,
			[](const FHitResult&, FString& Reason, FHBSpawnCandidateReport& Candidate)
			{ Candidate.Cause = EHBSpawnFailureReason::ExcludedArea; Reason = TEXT("Original is excluded"); return false; }, Floor, Error, Report));
	TestEqual(TEXT("Candidates outside XY bounds are counted without traces"), Report.CountFailureReasons().FindRef(EHBSpawnFailureReason::OutsideRoom), 24);
	TestFalse(TEXT("Outside XY candidate has no fabricated floor"), Report.Candidates[1].bHasFloor);

	Strip->GetOwner()->SetActorLocationAndRotation(FVector::ZeroVector, FRotator(60, 0, 0));
	TestFalse(TEXT("Physical slope failures have their own reason"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f,
			Query, Accept, Floor, Error, Report));
	TestEqual(TEXT("Slope count is separate from penetration"), Report.CountFailureReasons().FindRef(EHBSpawnFailureReason::SteepGround), 25);
	Strip->GetOwner()->SetActorLocationAndRotation(FVector(0, 0, 100), FRotator::ZeroRotator);
	Strip->SetBoxExtent(FVector(500, 500, 20));
	TestFalse(TEXT("A trace beginning inside physical ground remains rejected"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f,
			Query, Accept, Floor, Error, Report));
	TestEqual(TEXT("Penetration is counted separately from slope"), Report.CountFailureReasons().FindRef(EHBSpawnFailureReason::GroundPenetration), 25);

	Strip->GetOwner()->SetActorLocation(FVector(0, 0, -10));
	Strip->SetBoxExtent(FVector(500, 500, 10));
	TestTrue(TEXT("A successful reuse clears previous failed candidates"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, Bounds, 0.7f,
			Query, Accept, Floor, Error, Report));
	TestEqual(TEXT("Valid original stops at candidate zero"), Report.AcceptedCandidateIndex, 0);
	TestEqual(TEXT("No untested candidate is recorded"), Report.Candidates.Num(), 1);
	TestTrue(TEXT("Success contains no failure counts"), Report.CountFailureReasons().IsEmpty());
	TestFalse(TEXT("Invalid search input has no invented candidate"),
		HBSpawnGroundQuery::FindPlacementWithDiagnostics(*TestWorld.World, Marker, FQuat::Identity, FBox(ForceInit), 0.7f,
			Query, Accept, Floor, Error, Report));
	TestTrue(TEXT("Invalid search configuration has a terminal typed reason"),
		Report.FailureReason == EHBSpawnFailureReason::InvalidConfiguration && Report.Candidates.IsEmpty());
	return true;
}
#endif
