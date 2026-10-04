#include "PCGModule/HBSpawnCountSelection.h"
#include "PCGModule/HBEncounterComponent.h"
#include "PCGModule/HBChestPlacementComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "Loot/BoxActor.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnCountSelectionTest, "HeavenBreach.PCGModule.SpawnCount.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnCountSelectionTest::RunTest(const FString&)
{
	FString Error;
	int32 Count = 0;
	const TArray<FHBSpawnCountWeight> IgnoredWeights{{999, -1}};
	for (int32 Seed : {1, 17, 12345})
		for (const FIntPoint Range : {FIntPoint(0, 0), FIntPoint(1, 1), FIntPoint(2, 7)})
		{
			FRandomStream Original(Seed), Updated(Seed);
			for (int32 Draw = 0; Draw < 64; ++Draw)
			{
				const int32 Expected = Original.RandRange(Range.X, Range.Y);
				TestTrue(TEXT("Uniform draw succeeds"), HBSpawnCountSelection::Select(Range.X, Range.Y,
					EHBSpawnCountSelectionMode::Uniform, IgnoredWeights, Updated, Count, Error));
				TestEqual(TEXT("Uniform mode preserves the existing count"), Count, Expected);
				TestEqual(TEXT("Uniform mode preserves subsequent random decisions"), Updated.GetCurrentSeed(), Original.GetCurrentSeed());
			}
		}

	TArray<FHBSpawnCountWeight> Weights{{5, 6}, {2, 0}, {1, 1}, {3, 3}};
	TArray<FHBSpawnCountWeight> Reordered = Weights;
	Algo::Reverse(Reordered);
	FRandomStream First(8127), Replay(8127);
	TMap<int32, int32> Histogram;
	for (int32 Draw = 0; Draw < 12000; ++Draw)
	{
		int32 ReplayedCount = 0;
		if (!HBSpawnCountSelection::Select(1, 5, EHBSpawnCountSelectionMode::Weighted,
			Weights, First, Count, Error)
			|| !HBSpawnCountSelection::Select(1, 5, EHBSpawnCountSelectionMode::Weighted,
				Reordered, Replay, ReplayedCount, Error))
		{ AddError(Error); return false; }
		if (Count != ReplayedCount)
		{ AddError(TEXT("Weighted count changed after reordering rows.")); return false; }
		++Histogram.FindOrAdd(Count);
	}
	TestFalse(TEXT("Zero-weight count is never selected"), Histogram.Contains(2));
	TestFalse(TEXT("Omitted count is never selected"), Histogram.Contains(4));
	TestTrue(TEXT("Min count uses its relative weight"), FMath::Abs(Histogram.FindRef(1) / 12000.f - 0.1f) < 0.02f);
	TestTrue(TEXT("Middle count uses its relative weight"), FMath::Abs(Histogram.FindRef(3) / 12000.f - 0.3f) < 0.02f);
	TestTrue(TEXT("Max count uses its explicit relative weight"), FMath::Abs(Histogram.FindRef(5) / 12000.f - 0.6f) < 0.02f);
	TestEqual(TEXT("Weighted replay also preserves the stream state"), First.GetCurrentSeed(), Replay.GetCurrentSeed());
	int32 Minimum = 0, Maximum = 0;
	TestTrue(TEXT("Selectable range is available"), HBSpawnCountSelection::GetSelectableRange(1, 5,
		EHBSpawnCountSelectionMode::Weighted, {{1, 0}, {2, 4}, {4, 1}, {5, 0}}, Minimum, Maximum));
	TestEqual(TEXT("Zero-weight lower endpoint is excluded"), Minimum, 2);
	TestEqual(TEXT("Zero-weight upper endpoint is excluded"), Maximum, 4);
	TestTrue(TEXT("Zero count can be explicitly selected"), HBSpawnCountSelection::Select(0, 3,
		EHBSpawnCountSelectionMode::Weighted, {{0, 1}, {3, 0}}, First, Count, Error));
	TestEqual(TEXT("Zero count result"), Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnCountValidationTest, "HeavenBreach.PCGModule.SpawnCount.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnCountValidationTest::RunTest(const FString&)
{
	TArray<FString> Errors;
	auto Validate = [&Errors](const TArray<FHBSpawnCountWeight>& Weights)
	{
		return HBSpawnCountSelection::Validate(1, 3, EHBSpawnCountSelectionMode::Weighted, Weights, Errors);
	};
	TestTrue(TEXT("Relative weights need not sum to 100"), Validate({{1, 1}, {2, 2}, {3, 1}}));
	TestFalse(TEXT("Weighted mode requires entries"), Validate({}));
	TestFalse(TEXT("All-zero weights are rejected"), Validate({{1, 0}, {3, 0}}));
	TestFalse(TEXT("Out-of-range count is rejected"), Validate({{4, 1}}));
	TestFalse(TEXT("Duplicate count is rejected"), Validate({{1, 1}, {1, 2}}));
	TestFalse(TEXT("Negative weight is rejected"), Validate({{1, -1}, {3, 1}}));
	TestFalse(TEXT("Weight sum overflow is rejected"), Validate({{1, MAX_int32}, {3, 1}}));
	TestFalse(TEXT("Inverted range is rejected"), HBSpawnCountSelection::Validate(3, 1,
		EHBSpawnCountSelectionMode::Weighted, {{3, 1}}, Errors));
	TestFalse(TEXT("Invalid mode is rejected"), HBSpawnCountSelection::Validate(1, 3,
		static_cast<EHBSpawnCountSelectionMode>(255), {}, Errors));
	FRandomStream Random(12345);
	const int32 BeforeFailure = Random.GetCurrentSeed();
	int32 Count = 0;
	FString Error;
	TestFalse(TEXT("Invalid weights cannot silently select a count"), HBSpawnCountSelection::Select(1, 3,
		EHBSpawnCountSelectionMode::Weighted, {}, Random, Count, Error));
	TestEqual(TEXT("Validation failure does not consume the seed stream"), Random.GetCurrentSeed(), BeforeFailure);

	UHBEncounterComponent* Encounter = NewObject<UHBEncounterComponent>(GetTransientPackage());
	FHBEncounterGroup EnemyGroup;
	EnemyGroup.GroupId = TEXT("Common");
	EnemyGroup.MinCount = 1;
	EnemyGroup.MaxCount = 3;
	EnemyGroup.CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
	EnemyGroup.CountWeights = {{1, 1}, {3, 4}};
	EnemyGroup.Monsters.Add({AEnemyCharacterBase::StaticClass(), 1});
	Encounter->Groups.Add(EnemyGroup);
	TestTrue(TEXT("Monster component accepts count weights"), Encounter->Validate(Errors));
	Encounter->Groups[0].CountWeights = {{3, 0}};
	TestFalse(TEXT("Monster component reports invalid count weights"), Encounter->Validate(Errors));
	Encounter->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Uniform;
	TestTrue(TEXT("Unused weights do not change uniform monster settings"), Encounter->Validate(Errors));

	UHBChestPlacementComponent* Chest = NewObject<UHBChestPlacementComponent>(GetTransientPackage());
	FHBChestPlacementGroup ChestGroup;
	ChestGroup.GroupId = TEXT("Loot");
	ChestGroup.MinCount = 1;
	ChestGroup.MaxCount = 3;
	ChestGroup.CountSelectionMode = EHBSpawnCountSelectionMode::Weighted;
	ChestGroup.CountWeights = {{1, 1}, {3, 4}};
	ChestGroup.Chests.Add({ABoxActor::StaticClass(), 1});
	Chest->Groups.Add(ChestGroup);
	TestTrue(TEXT("Chest component accepts count weights"), Chest->Validate(Errors));
	Chest->Groups[0].CountWeights.Add({3, 1});
	TestFalse(TEXT("Chest component reports duplicate count weights"), Chest->Validate(Errors));
	Chest->Groups[0].CountSelectionMode = EHBSpawnCountSelectionMode::Uniform;
	TestTrue(TEXT("Unused weights do not change uniform chest settings"), Chest->Validate(Errors));
	return true;
}
#endif
