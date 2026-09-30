#include "PCGModule/HBChestPlacementComponent.h"
#include "PCGModule/HBChestSetupActor.h"
#include "PCGModule/HBChestSpawnPoint.h"
#include "Loot/BoxActor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBChestPlacementAuthoringTest,
	"HeavenBreach.PCGModule.ChestPlacement.Authoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBChestPlacementAuthoringTest::RunTest(const FString&)
{
	const AHBChestSetupActor* Setup = AHBChestSetupActor::StaticClass()->GetDefaultObject<AHBChestSetupActor>();
	TestNotNull(TEXT("Placeable chest setup owns the settings component"), Setup->Placement.Get());
	const AHBChestSpawnPoint* Point = AHBChestSpawnPoint::StaticClass()->GetDefaultObject<AHBChestSpawnPoint>();
	TestNotNull(TEXT("Placeable chest point owns a direction marker"), Point->Direction.Get());

	UHBChestPlacementComponent* Config = NewObject<UHBChestPlacementComponent>(GetTransientPackage());
	FHBChestPlacementGroup Random;
	Random.GroupId = TEXT("Random");
	Random.MinCount = 1;
	Random.MaxCount = 3;
	Random.Chests.Add({ABoxActor::StaticClass(), 4});
	Config->Groups.Add(Random);
	FHBChestPlacementGroup Guaranteed;
	Guaranteed.GroupId = TEXT("Guaranteed");
	Guaranteed.MinCount = Guaranteed.MaxCount = 1;
	Guaranteed.Chests.Add({ABoxActor::StaticClass(), 1});
	Config->Groups.Add(Guaranteed);
	TArray<FString> Errors;
	TestTrue(TEXT("Ranged and fixed groups with weighted box classes are valid"), Config->Validate(Errors));

	Config->Groups[1].GroupId = Random.GroupId;
	TestFalse(TEXT("Duplicate group ID is rejected"), Config->Validate(Errors));
	Config->Groups[1].GroupId = TEXT("Guaranteed");
	Config->Groups[0].MaxCount = 0;
	TestFalse(TEXT("Inverted count range is rejected"), Config->Validate(Errors));
	Config->Groups[0].MaxCount = 3;
	Config->Groups[0].Chests[0].Weight = 0;
	TestFalse(TEXT("Nonpositive weight is rejected"), Config->Validate(Errors));
	Config->Groups[0].Chests[0].Weight = 4;
	Config->Groups[0].Chests.Add({ABoxActor::StaticClass(), 1});
	TestFalse(TEXT("Duplicate box class in one group is rejected"), Config->Validate(Errors));
	Config->Groups[0].Chests.Empty();
	TestFalse(TEXT("Positive count needs a box class"), Config->Validate(Errors));
	return true;
}
#endif
