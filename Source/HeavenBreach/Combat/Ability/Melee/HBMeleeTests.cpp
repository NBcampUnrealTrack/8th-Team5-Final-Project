#include "Combat/Ability/Melee/HBMelee.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMeleeBloodthirstTest, "HeavenBreach.Melee.Bloodthirst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMeleeBloodthirstTest::RunTest(const FString&)
{
	TestEqual(TEXT("패시브 없음"), HBMelee::BloodthirstHealRatio(false, 3), 0.f);
	TestEqual(TEXT("기본"), HBMelee::BloodthirstHealRatio(true, 0), 0.05f);
	TestEqual(TEXT("음수 단계는 기본"), HBMelee::BloodthirstHealRatio(true, -1), 0.05f);
	TestEqual(TEXT("강화1"), HBMelee::BloodthirstHealRatio(true, 1), 0.07f);
	TestEqual(TEXT("강화2"), HBMelee::BloodthirstHealRatio(true, 2), 0.09f);
	TestEqual(TEXT("강화3"), HBMelee::BloodthirstHealRatio(true, 3), 0.12f);
	TestEqual(TEXT("강화 초과"), HBMelee::BloodthirstHealRatio(true, 9), 0.12f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMeleeSteadyStanceTest, "HeavenBreach.Melee.SteadyStance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMeleeSteadyStanceTest::RunTest(const FString&)
{
	TestEqual(TEXT("패시브 없음"), HBMelee::SteadyStanceMultiplier(false, 3), 1.f);
	TestEqual(TEXT("기본"), HBMelee::SteadyStanceMultiplier(true, 0), 0.70f);
	TestEqual(TEXT("음수 단계는 기본"), HBMelee::SteadyStanceMultiplier(true, -2), 0.70f);
	TestEqual(TEXT("강화1"), HBMelee::SteadyStanceMultiplier(true, 1), 0.60f);
	TestEqual(TEXT("강화2"), HBMelee::SteadyStanceMultiplier(true, 2), 0.50f);
	TestEqual(TEXT("강화3"), HBMelee::SteadyStanceMultiplier(true, 3), 0.40f);
	TestEqual(TEXT("강화 초과"), HBMelee::SteadyStanceMultiplier(true, 9), 0.40f);
	return true;
}
#endif
