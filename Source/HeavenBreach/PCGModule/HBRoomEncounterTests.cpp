#include "PCGModule/HBRoomEncounterComponent.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "AI/EnemyCharacterBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRoomEncounterSelectionTest, "HeavenBreach.PCGModule.EncounterSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBRoomEncounterSelectionTest::RunTest(const FString&)
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
	auto* Profile = NewObject<UHBRoomEncounterProfile>(GetTransientPackage());
	Profile->MinCommonCount = 3;
	Profile->MaxCommonCount = 3;
	FHBEnemySpawnOption Option;
	Option.EnemyClass = AEnemyCharacterBase::StaticClass();
	Profile->CommonEnemies.Add(Option);
	TArray<FHBEncounterSpawnChoice> Placed;
	FString Error;
	auto Accept = [](const FHBEncounterSpawnChoice&, FString&) { return true; };
	auto Signature = [](const TArray<FHBEncounterSpawnChoice>& Choices)
	{
		FString Result;
		for (const auto& Choice : Choices) Result += Choice.PointId.ToString() + TEXT("|");
		return Result;
	};
	TestTrue(TEXT("Select three common monsters"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exact requested count"), Placed.Num(), 3);
	TestTrue(TEXT("Required point selected"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[0]->PointId; }));
	const FString FirstSignature = Signature(Placed);
	Algo::Reverse(Points);
	TestTrue(TEXT("Reordered actor enumeration accepted"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Same seed and room yield the same placement"), Signature(Placed), FirstSignature);
	TSet<FString> RoomSignatures;
	for (int32 RoomId = 0; RoomId < 8; ++RoomId)
	{
		TestTrue(TEXT("Reused module plans independently for each room"),
			HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, RoomId, Accept, Placed, Error));
		RoomSignatures.Add(Signature(Placed));
	}
	TestTrue(TEXT("Different room IDs can change the placement"), RoomSignatures.Num() > 1);

	FName RejectedPoint;
	const FName RequiredId = Points.Last()->PointId;
	auto RejectOnePoint = [&RejectedPoint, RequiredId](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.PointId == RequiredId) return true;
		if (RejectedPoint.IsNone()) RejectedPoint = Choice.PointId;
		if (Choice.PointId == RejectedPoint) { Reason = TEXT("Unsafe candidate"); return false; }
		return true;
	};
	TestTrue(TEXT("Rejected point is replaced by another candidate"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, RejectOnePoint, Placed, Error));
	TestEqual(TEXT("Minimum survives one rejection"), Placed.Num(), 3);
	TestFalse(TEXT("Rejected point is absent"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == RejectedPoint; }));
	auto RejectAll = [](const FHBEncounterSpawnChoice&, FString& Reason)
	{
		Reason = TEXT("No safe floor");
		return false;
	};
	TestFalse(TEXT("Insufficient required count fails with bounded attempts"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, RejectAll, Placed, Error));
	TestTrue(TEXT("Failure names room and reason"), Error.Contains(TEXT("Room 2")) && Error.Contains(TEXT("No safe floor")));

	Profile->MinCommonCount = 0;
	Profile->MaxCommonCount = 0;
	Profile->CommonEnemies.Reset();
	Profile->BossClass = AEnemyCharacterBase::StaticClass();
	Profile->BossPointId = Points.Last()->PointId;
	Points.Last()->bAllowCommon = false;
	Points.Last()->bAllowBoss = true;
	TestTrue(TEXT("Boss is placed only at its dedicated point"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exactly one boss"), Placed.Num(), 1);
	if (Placed.Num() == 1)
	{
		TestEqual(TEXT("Boss point ID"), Placed[0].PointId, Profile->BossPointId);
		TestEqual(TEXT("Boss grade"), Placed[0].Grade, EHBEnemySpawnGrade::Boss);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
