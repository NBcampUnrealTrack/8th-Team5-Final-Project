#include "PCGPrototype/HBDungeonLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonSeedsTest, "HeavenBreach.PCGPrototype.SeedsAndRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonSeedsTest::RunTest(const FString&)
{
	TSet<FString> Signatures;
	for (int32 Seed = 0; Seed < 256; ++Seed)
	{
		HBDungeonLayout::FSettings S; S.Seed = Seed;
		TArray<FHBPrototypeRoom> A, B; FString Error;
		if (!TestTrue(FString::Printf(TEXT("Seed %d generates"), Seed), HBDungeonLayout::Generate(S,A,Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Same seed regenerates"), HBDungeonLayout::Generate(S,B,Error));
		TestEqual(TEXT("Deterministic replay"), HBDungeonLayout::Signature(A), HBDungeonLayout::Signature(B));
		Signatures.Add(HBDungeonLayout::Signature(A));
		int32 BossCount = 0; int32 Heal[2] = {}, Upgrade[2] = {};
		for (const auto& R : A)
		{
			BossCount += R.Role == EHBPrototypeRoomRole::Boss;
			Heal[R.Floor-1] += R.Role == EHBPrototypeRoomRole::Recovery;
			Upgrade[R.Floor-1] += R.Role == EHBPrototypeRoomRole::Upgrade;
			TSet<int32> Visited;
			for (int32 I = R.Id; I != INDEX_NONE; I = A[I].ParentId)
			{
				if (Visited.Contains(I)) { AddError(TEXT("Cycle")); return false; }
				Visited.Add(I);
			}
			TestTrue(TEXT("Every room reaches start"), Visited.Contains(0));
		}
		TestEqual(TEXT("Exactly one boss"), BossCount, 1);
		for (int32 I=0;I<2;++I) { TestTrue(TEXT("Healing cap"), Heal[I]<=1); TestTrue(TEXT("Upgrade cap"), Upgrade[I]<=1); }
	}
	TestTrue(TEXT("Different seeds produce diverse layouts"), Signatures.Num()>240);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDungeonInvalidTest, "HeavenBreach.PCGPrototype.RejectInvalidLayouts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDungeonInvalidTest::RunTest(const FString&)
{
	HBDungeonLayout::FSettings S; TArray<FHBPrototypeRoom> R; FString Error;
	TestTrue(TEXT("Baseline"), HBDungeonLayout::Generate(S,R,Error));
	auto Bad = R; Bad[1].ParentId = Bad[1].Id;
	TestFalse(TEXT("Cycle rejected"), HBDungeonLayout::Validate(Bad,Error));
	Bad = R; Bad[1].Bounds = Bad[0].Bounds;
	TestFalse(TEXT("Overlap rejected"), HBDungeonLayout::Validate(Bad,Error));
	Bad = R; Bad[1].Role = EHBPrototypeRoomRole::Recovery;
	TestFalse(TEXT("Recovery after start rejected"), HBDungeonLayout::Validate(Bad,Error));
	Bad = R; Bad[0].Role = EHBPrototypeRoomRole::Boss;
	TestFalse(TEXT("Bad start rejected"), HBDungeonLayout::Validate(Bad,Error));
	S.CellSize = 1; S.MaxAttempts = 2;
	TestFalse(TEXT("Impossible spacing fails within attempt budget"), HBDungeonLayout::Generate(S,R,Error));
	TestTrue(TEXT("No partial result on failure"), R.IsEmpty());
	return true;
}
#endif
