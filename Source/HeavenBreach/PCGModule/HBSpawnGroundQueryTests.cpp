#include "PCGModule/HBSpawnGroundQuery.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
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
#endif
