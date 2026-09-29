#include "PCGModule/HBEncounterComponent.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "AI/EnemyCharacterBase.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterAuthoringComponentTest,
	"HeavenBreach.PCGModule.EncounterComponent.Authoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterAuthoringComponentTest::RunTest(const FString&)
{
	const AHBEncounterSetupActor* Setup = AHBEncounterSetupActor::StaticClass()->GetDefaultObject<AHBEncounterSetupActor>();
	TestNotNull(TEXT("Placeable setup actor owns the settings component"), Setup->Encounter.Get());

	UHBEncounterComponent* Encounter = NewObject<UHBEncounterComponent>(GetTransientPackage());
	FHBEncounterGroup Common;
	Common.GroupId = TEXT("Common");
	Common.Grade = EHBEncounterGrade::Common;
	Common.MinCount = 2;
	Common.MaxCount = 4;
	Common.Monsters.Add({AEnemyCharacterBase::StaticClass(), 3});
	Encounter->Groups.Add(Common);
	FHBEncounterGroup Elite;
	Elite.GroupId = TEXT("Elite");
	Elite.Grade = EHBEncounterGrade::Elite;
	Elite.MinCount = Elite.MaxCount = 1;
	Elite.Monsters.Add({AEnemyCharacterBase::StaticClass(), 1});
	Encounter->Groups.Add(Elite);
	TArray<FString> Errors;
	TestTrue(TEXT("Fixed and ranged groups with weights are valid"), Encounter->Validate(Errors));
	TestEqual(TEXT("Configured fixed elite count"), Encounter->Groups[1].MinCount, Encounter->Groups[1].MaxCount);

	Encounter->Groups[1].GroupId = Common.GroupId;
	TestFalse(TEXT("Duplicate group ID is rejected"), Encounter->Validate(Errors));
	Encounter->Groups[1].GroupId = TEXT("Elite");
	Encounter->Groups[0].MaxCount = 1;
	TestFalse(TEXT("Inverted count range is rejected"), Encounter->Validate(Errors));
	Encounter->Groups[0].MaxCount = 4;
	Encounter->Groups[0].Monsters[0].Weight = 0;
	TestFalse(TEXT("Nonpositive weight is rejected"), Encounter->Validate(Errors));
	Encounter->Groups[0].Monsters[0].Weight = 3;
	Encounter->Groups[0].Monsters.Empty();
	TestFalse(TEXT("Positive count needs a monster class"), Encounter->Validate(Errors));
	return true;
}
#endif
