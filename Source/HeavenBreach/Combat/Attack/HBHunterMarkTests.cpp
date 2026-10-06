#include "Combat/Attack/HBHunterMark.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBHunterMarkBonusTest, "HeavenBreach.Bow.HunterMarkBonus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBHunterMarkBonusTest::RunTest(const FString&)
{
	FGameplayTagContainer Tags;
	TestEqual(TEXT("빈 컨테이너"), HBHunterMark::DamageBonus(Tags), 0.f);

	Tags.AddTag(ArcherTag::TAG_State_Tracked);
	TestEqual(TEXT("기본 표식"), HBHunterMark::DamageBonus(Tags), 0.20f);

	Tags.AddTag(ArcherTag::TAG_State_Tracked_Deep2);
	TestEqual(TEXT("Tracked + Deep2"), HBHunterMark::DamageBonus(Tags), 0.30f);

	FGameplayTagContainer Mixed;
	Mixed.AddTag(ArcherTag::TAG_State_Tracked_Deep1);
	Mixed.AddTag(ArcherTag::TAG_State_Tracked_Deep3);
	TestEqual(TEXT("가장 큰 값"), HBHunterMark::DamageBonus(Mixed), 0.35f);

	TestFalse(TEXT("0.20 은 기본 표식"), HBHunterMark::TagForBonus(0.20f).IsValid());
	TestEqual(TEXT("0.25"), HBHunterMark::TagForBonus(0.25f), ArcherTag::TAG_State_Tracked_Deep1.GetTag());
	TestEqual(TEXT("0.30"), HBHunterMark::TagForBonus(0.30f), ArcherTag::TAG_State_Tracked_Deep2.GetTag());
	TestEqual(TEXT("0.35"), HBHunterMark::TagForBonus(0.35f), ArcherTag::TAG_State_Tracked_Deep3.GetTag());
	return true;
}

#endif
