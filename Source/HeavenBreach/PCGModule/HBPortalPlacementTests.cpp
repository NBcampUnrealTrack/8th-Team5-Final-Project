#include "PCGModule/HBPortalPlacement.h"
#include "PCGModule/HBPortalSpawnPoint.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Algo/Reverse.h"
#include "Components/ArrowComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	using namespace HBPortalPlacement;

	FCandidate Candidate(int32 RoomId, int32 Floor, const TCHAR* PointId)
	{
		return {{RoomId, FName(PointId)}, Floor};
	}

	TArray<FCandidate> RepeatedRoomCandidates()
	{
		TArray<FCandidate> Result;
		for (int32 Floor = 1; Floor <= 2; ++Floor)
			for (int32 Room = 0; Room < 4; ++Room)
				for (int32 Point = 0; Point < 3; ++Point)
					Result.Add({{Floor * 100 + Room, FName(*FString::Printf(TEXT("P%d"), Point))}, Floor});
		return Result;
	}

	FString Signature(const TArray<FPair>& Pairs)
	{
		FString Result;
		for (const FPair& Pair : Pairs)
			Result += FString::Printf(TEXT("%d|%d:%s->%d:%s;"), Pair.PairId,
				Pair.Floor1.RoomId, *Pair.Floor1.PointId.ToString(), Pair.Floor2.RoomId, *Pair.Floor2.PointId.ToString());
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalPlacementAuthoringTest,
	"HeavenBreach.PCGModule.PortalPlacement.Authoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalPlacementAuthoringTest::RunTest(const FString&)
{
	// Loading the existing BP verifies inheritance without changing or saving the asset.
	UClass* GeneratorClass = LoadClass<AHBDungeonRuntime>(nullptr, TEXT("/Game/Core/PCG/BP_HBDungeonRuntime.BP_HBDungeonRuntime_C"));
	if (!TestNotNull(TEXT("Existing generator BP loads with portal settings"), GeneratorClass)) return false;
	const auto* Defaults = GeneratorClass->GetDefaultObject<AHBDungeonRuntime>();
	TestTrue(TEXT("Existing BP keeps fixed room portal behavior by default"),
		Defaults->PortalPlacementMode == EHBDungeonPortalPlacementMode::FixedRoomPortals);
	TestEqual(TEXT("Initial random pair count is one"), Defaults->PortalPairCount, 1);
	for (const TCHAR* Name : {TEXT("PortalPlacementMode"), TEXT("PortalClass"), TEXT("PortalPairCount")})
	{
		const FProperty* Property = FindFProperty<FProperty>(GeneratorClass, FName(Name));
		if (!TestNotNull(FString::Printf(TEXT("BP exposes %s"), Name), Property)) continue;
		TestTrue(TEXT("Portal setting is writable in BP defaults and placed instances"),
			Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
			&& !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly | CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate));
	}
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("Authoring test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	auto* Generator = World->SpawnActor<AHBDungeonRuntime>(GeneratorClass);
	if (!TestNotNull(TEXT("Generator BP instance for effective portal rules"), Generator)) return false;
	Generator->PortalPlacementMode = EHBDungeonPortalPlacementMode::RandomPoints;
	Generator->PortalPairCount = 3;
	const auto Rules = Generator->GetGenerationRules();
	TestTrue(TEXT("Generation uses the directly edited BP portal mode"), Rules.PortalPlacementMode == EHBDungeonPortalPlacementMode::RandomPoints);
	TestEqual(TEXT("Generation uses the directly edited BP pair count"), Rules.PortalPairCount, 3);
	TestEqual(TEXT("Portal settings retain authored floor counts"), Rules.Floor1MainRooms, Generator->DungeonRules.Floor1MainRooms);
	auto* Point = World->SpawnActor<AHBPortalSpawnPoint>();
	if (!TestNotNull(TEXT("Placeable portal marker"), Point)) return false;
	if (!TestNotNull(TEXT("Marker owns its direction arrow"), Point->Direction.Get())) return false;
	TestFalse(TEXT("Candidate marker does not tick"), Point->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Candidate marker does not replicate"), Point->GetIsReplicated());
	TestTrue(TEXT("Candidate marker has no collision"), Point->Direction->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Candidate marker does not alter navigation"), Point->Direction->CanEverAffectNavigation());
	TestTrue(TEXT("Candidate marker is hidden in game"), Point->Direction->bHiddenInGame);
	FString Error;
	TestFalse(TEXT("LD must assign a point ID"), Point->ValidateDefinition(Error));
	Point->PointId = TEXT("PortalCandidate1");
	Point->SetActorLocationAndRotation(FVector(100, 200, 300), FRotator(0, 90, 0));
	TestTrue(TEXT("Positioned and rotated marker is valid"), Point->ValidateDefinition(Error));
	TestTrue(TEXT("Successful validation clears old error"), Error.IsEmpty());
	Point->SetActorScale3D(FVector(2));
	TestFalse(TEXT("Scaling the marker cannot silently scale portal and arrival"), Point->ValidateDefinition(Error));
	Point->SetActorScale3D(FVector::OneVector);
	TestTrue(TEXT("Restoring unit scale fixes the definition"), Point->ValidateDefinition(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalPlacementInvalidInputTest,
	"HeavenBreach.PCGModule.PortalPlacement.InvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalPlacementInvalidInputTest::RunTest(const FString&)
{
	const TArray<FCandidate> Valid = {
		Candidate(0, 1, TEXT("P1")), Candidate(0, 1, TEXT("P2")),
		Candidate(1, 2, TEXT("P1")), Candidate(1, 2, TEXT("P2"))};
	TArray<FPair> Pairs;
	FString Error;
	auto ExpectFailure = [&](const TCHAR* Label, int32 Count, const TArray<FCandidate>& Input, const TCHAR* Reason)
	{
		TestTrue(TEXT("Preload a previous successful result"), SelectPairs(17, 1, Valid, Pairs, Error));
		TestFalse(Label, SelectPairs(17, Count, Input, Pairs, Error));
		TestEqual(TEXT("Failure clears all previous pairs"), Pairs.Num(), 0);
		TestTrue(TEXT("Failure reports its cause"), Error.Contains(Reason));
	};
	ExpectFailure(TEXT("Zero pairs are rejected"), 0, Valid, TEXT("at least 1"));
	ExpectFailure(TEXT("Negative pairs are rejected"), -1, Valid, TEXT("at least 1"));
	ExpectFailure(TEXT("Empty candidates are rejected"), 1, {}, TEXT("Floor1=0, Floor2=0"));
	ExpectFailure(TEXT("Huge request fails before allocating pairs"), MAX_int32, Valid, TEXT("Insufficient"));
	TArray<FCandidate> Invalid = Valid;
	Invalid.Pop();
	ExpectFailure(TEXT("Second floor lacks a candidate"), 2, Invalid, TEXT("Floor1=2, Floor2=1"));
	Invalid = Valid;
	Invalid.RemoveAt(0);
	ExpectFailure(TEXT("First floor lacks a candidate"), 2, Invalid, TEXT("Floor1=1, Floor2=2"));
	Invalid = Valid;
	Invalid[0].Key.RoomId = INDEX_NONE;
	ExpectFailure(TEXT("Missing room identity"), 1, Invalid, TEXT("RoomId"));
	Invalid = Valid;
	Invalid[0].Key.PointId = NAME_None;
	ExpectFailure(TEXT("Missing point identity"), 1, Invalid, TEXT("PointId"));
	Invalid = Valid;
	Invalid[0].Floor = 3;
	ExpectFailure(TEXT("Unknown logical floor"), 1, Invalid, TEXT("unsupported floor 3"));
	Invalid = Valid;
	Invalid.Add(Valid[0]);
	ExpectFailure(TEXT("Duplicate room-local point"), 1, Invalid, TEXT("Duplicate"));
	Invalid.Last().Floor = 2;
	ExpectFailure(TEXT("A point cannot belong to both floors"), 1, Invalid, TEXT("Duplicate"));
	Invalid = Valid;
	Invalid[1].Key.PointId = TEXT("p1");
	ExpectFailure(TEXT("Point IDs use FName case-insensitive identity"), 1, Invalid, TEXT("Duplicate"));
	TestTrue(TEXT("Valid input recovers after failures"), SelectPairs(17, 2, Valid, Pairs, Error));
	TestTrue(TEXT("Successful selection clears previous error"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalPlacementSeedInvariantTest,
	"HeavenBreach.PCGModule.PortalPlacement.SeedInvariants1000",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalPlacementSeedInvariantTest::RunTest(const FString&)
{
	const TArray<FCandidate> Candidates = RepeatedRoomCandidates();
	TMap<FPointKey, int32> Floors;
	for (const FCandidate& Point : Candidates) Floors.Add(Point.Key, Point.Floor);
	TSet<FString> DifferentSelections;
	for (int32 Seed = 0; Seed < 1000; ++Seed)
	{
		for (int32 Count : {1, 5, 12})
		{
			TArray<FPair> Pairs;
			FString Error;
			if (!TestTrue(FString::Printf(TEXT("Seed=%d Count=%d selects successfully"), Seed, Count),
				SelectPairs(Seed, Count, Candidates, Pairs, Error))) return false;
			TestEqual(TEXT("Exact requested pair count"), Pairs.Num(), Count);
			TSet<FPointKey> Used;
			for (int32 Index = 0; Index < Pairs.Num(); ++Index)
			{
				const FPair& Pair = Pairs[Index];
				TestEqual(TEXT("Stable generation-local pair index"), Pair.PairId, Index);
				const int32* Floor1 = Floors.Find(Pair.Floor1);
				const int32* Floor2 = Floors.Find(Pair.Floor2);
				if (!TestNotNull(TEXT("First endpoint comes from candidates"), Floor1)
					|| !TestNotNull(TEXT("Second endpoint comes from candidates"), Floor2)) return false;
				TestEqual(TEXT("First endpoint belongs to logical floor 1"), *Floor1, 1);
				TestEqual(TEXT("Second endpoint belongs to logical floor 2"), *Floor2, 2);
				TestFalse(TEXT("First endpoint is never reused"), Used.Contains(Pair.Floor1));
				Used.Add(Pair.Floor1);
				TestFalse(TEXT("Second endpoint is never reused"), Used.Contains(Pair.Floor2));
				Used.Add(Pair.Floor2);
			}
			TestEqual(TEXT("N pairs use exactly 2N distinct points"), Used.Num(), Count * 2);
			if (Count == 5) DifferentSelections.Add(Signature(Pairs));
		}
	}
	TestTrue(TEXT("Different seeds produce different selections and pairings"), DifferentSelections.Num() > 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalPlacementDeterminismTest,
	"HeavenBreach.PCGModule.PortalPlacement.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalPlacementDeterminismTest::RunTest(const FString&)
{
	const TArray<FCandidate> Candidates = RepeatedRoomCandidates();
	TArray<FCandidate> Reordered = Candidates;
	Algo::Reverse(Reordered);
	for (int32 Seed : {0, 42, -1, MIN_int32, MAX_int32})
	{
		TArray<FPair> Original, Replay, Reversed;
		FString Error;
		TestTrue(TEXT("Original selection"), SelectPairs(Seed, 5, Candidates, Original, Error));
		TestTrue(TEXT("Repeated selection"), SelectPairs(Seed, 5, Candidates, Replay, Error));
		TestTrue(TEXT("Reverse discovery order selection"), SelectPairs(Seed, 5, Reordered, Reversed, Error));
		TestEqual(TEXT("Same seed repeats exact pair IDs and endpoints"), Signature(Replay), Signature(Original));
		TestEqual(TEXT("Discovery order cannot change the result"), Signature(Reversed), Signature(Original));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalPlacementAnyOppositeFloorTest,
	"HeavenBreach.PCGModule.PortalPlacement.AnyOppositeFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalPlacementAnyOppositeFloorTest::RunTest(const FString&)
{
	// P1 has no fixed destination: every opposite-floor point must be a possible counterpart.
	const TArray<FCandidate> Candidates = {
		Candidate(0, 1, TEXT("P1")), Candidate(1, 2, TEXT("P2")),
		Candidate(2, 2, TEXT("P3")), Candidate(3, 2, TEXT("P4")), Candidate(4, 2, TEXT("P5"))};
	TSet<FPointKey> Destinations;
	for (int32 Seed = 0; Seed < 1000; ++Seed)
	{
		TArray<FPair> Pairs;
		FString Error;
		if (!TestTrue(TEXT("One cross-floor pair is available"), SelectPairs(Seed, 1, Candidates, Pairs, Error))) return false;
		if (!TestEqual(TEXT("One pair"), Pairs.Num(), 1)) return false;
		TestTrue(TEXT("P1 is the departure endpoint"), Pairs[0].Floor1 == Candidates[0].Key);
		Destinations.Add(Pairs[0].Floor2);
	}
	TestEqual(TEXT("P2, P3, P4 and P5 can all match P1 across seeds"), Destinations.Num(), 4);
	return true;
}
#endif
