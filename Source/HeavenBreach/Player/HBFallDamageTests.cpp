#include "Player/HBFallDamage.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBFallDamageComputeTest, "HeavenBreach.FallDamage.Compute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBFallDamageComputeTest::RunTest(const FString&)
{
	constexpr float Safe = 320.f;
	constexpr float Lethal = 1150.f;

	TestEqual(TEXT("안전 높이 이하는 0"), HBFallDamage::DamageRatio(320.f, Safe, Lethal), 0.f);
	TestEqual(TEXT("음수 높이(올라가며 착지)는 0"), HBFallDamage::DamageRatio(-50.f, Safe, Lethal), 0.f);
	TestEqual(TEXT("치명 높이는 1"), HBFallDamage::DamageRatio(1150.f, Safe, Lethal), 1.f);
	TestEqual(TEXT("치명 높이 넘어도 1"), HBFallDamage::DamageRatio(3000.f, Safe, Lethal), 1.f);
	TestEqual(TEXT("6m 는 약 41%"), HBFallDamage::DamageRatio(600.f, Safe, Lethal), 280.f / 830.f, 0.0001f);
	TestEqual(TEXT("잘못된 범위(치명<=안전)에서 안전 이하는 0"), HBFallDamage::DamageRatio(400.f, 500.f, 500.f), 0.f);
	TestEqual(TEXT("잘못된 범위에서 넘으면 1"), HBFallDamage::DamageRatio(600.f, 500.f, 500.f), 1.f);

	TestEqual(TEXT("감소 없음: 최대체력 200, 6m"), HBFallDamage::ComputeDamage(200.f, 600.f, Safe, Lethal, 0.f, 0.9f), 200.f * 280.f / 830.f, 0.001f);
	TestEqual(TEXT("감소 40%"), HBFallDamage::ComputeDamage(100.f, 1150.f, Safe, Lethal, 0.4f, 0.9f), 60.f, 0.001f);
	TestEqual(TEXT("감소는 상한 90% 로 묶임"), HBFallDamage::ComputeDamage(100.f, 1150.f, Safe, Lethal, 1.5f, 0.9f), 10.f, 0.001f);
	TestEqual(TEXT("음수 감소는 0 으로"), HBFallDamage::ComputeDamage(100.f, 1150.f, Safe, Lethal, -0.5f, 0.9f), 100.f, 0.001f);
	TestEqual(TEXT("안전 높이면 피해 0"), HBFallDamage::ComputeDamage(100.f, 300.f, Safe, Lethal, 0.f, 0.9f), 0.f);

	return true;
}

#endif
