#include "Combat/Ability/Magic/HBMagic.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicCastTimeTest, "HeavenBreach.Magic.CastTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicCastTimeTest::RunTest(const FString&)
{
	TestEqual(TEXT("집중 없음"), HBMagic::CastTimeMultiplier(false, 3), 1.f);
	TestEqual(TEXT("집중"), HBMagic::CastTimeMultiplier(true, 0), 0.75f);
	TestEqual(TEXT("강화1"), HBMagic::CastTimeMultiplier(true, 1), 0.65f);
	TestEqual(TEXT("강화2"), HBMagic::CastTimeMultiplier(true, 2), 0.55f);
	TestEqual(TEXT("강화3"), HBMagic::CastTimeMultiplier(true, 3), 0.45f);
	TestEqual(TEXT("강화 초과"), HBMagic::CastTimeMultiplier(true, 9), 0.45f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicResidualTest, "HeavenBreach.Magic.Residual", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicResidualTest::RunTest(const FString&)
{
	TArray<int32> Consumed;
	TestFalse(TEXT("ID 0"), HBMagic::ConsumeResidualTrigger(0, Consumed));
	TestFalse(TEXT("ID 음수"), HBMagic::ConsumeResidualTrigger(-3, Consumed));
	TestTrue(TEXT("첫 적중"), HBMagic::ConsumeResidualTrigger(5, Consumed));
	TestFalse(TEXT("같은 시전 두 번째"), HBMagic::ConsumeResidualTrigger(5, Consumed));
	TestTrue(TEXT("다음 시전"), HBMagic::ConsumeResidualTrigger(6, Consumed));
	TestFalse(TEXT("겹친 시전: 5,6 뒤 5 다시"), HBMagic::ConsumeResidualTrigger(5, Consumed));

	// 상한: 최근 MaxResidualConsumed 개만 남는다. 가장 오래된 5 가 밀려나면 다시 받는다.
	TArray<int32> Bounded;
	for (int32 Id = 1; Id <= HBMagic::MaxResidualConsumed + 1; ++Id)
	{
		HBMagic::ConsumeResidualTrigger(Id, Bounded);
	}
	TestEqual(TEXT("상한 유지"), Bounded.Num(), HBMagic::MaxResidualConsumed);
	TestFalse(TEXT("최근 ID 는 남음"), HBMagic::ConsumeResidualTrigger(HBMagic::MaxResidualConsumed + 1, Bounded));
	TestTrue(TEXT("가장 오래된 ID 는 밀려남"), HBMagic::ConsumeResidualTrigger(1, Bounded));
	TestEqual(TEXT("감소 기본"), HBMagic::ResidualReduction(0), 1.f);
	TestEqual(TEXT("감소 3단계"), HBMagic::ResidualReduction(3), 2.5f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicManaCycleTest, "HeavenBreach.Magic.ManaCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicManaCycleTest::RunTest(const FString&)
{
	TestEqual(TEXT("마력 순환 없음"), HBMagic::ManaCycleMultiplier(false, 3), 1.f);
	TestEqual(TEXT("마력 순환"), HBMagic::ManaCycleMultiplier(true, 0), 0.80f);
	TestEqual(TEXT("강화1"), HBMagic::ManaCycleMultiplier(true, 1), 0.70f);
	TestEqual(TEXT("강화2"), HBMagic::ManaCycleMultiplier(true, 2), 0.60f);
	TestEqual(TEXT("강화3"), HBMagic::ManaCycleMultiplier(true, 3), 0.50f);
	TestEqual(TEXT("강화 초과"), HBMagic::ManaCycleMultiplier(true, 9), 0.50f);
	TestEqual(TEXT("음수 단계는 기본"), HBMagic::ManaCycleMultiplier(true, -1), 0.80f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicOrbFalloffTest, "HeavenBreach.Magic.OrbFalloff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicOrbFalloffTest::RunTest(const FString&)
{
	// 원소 구체 폭발: 중심 100% → 가장자리 50%, 밖은 0.
	TestEqual(TEXT("중심 100%"), HBMagic::OrbExplosionFalloff(0.f, 200.f, 0.5f), 1.f);
	TestEqual(TEXT("절반 거리 75%"), HBMagic::OrbExplosionFalloff(100.f, 200.f, 0.5f), 0.75f);
	TestEqual(TEXT("가장자리 50%"), HBMagic::OrbExplosionFalloff(200.f, 200.f, 0.5f), 0.5f);
	TestEqual(TEXT("범위 밖 0"), HBMagic::OrbExplosionFalloff(201.f, 200.f, 0.5f), 0.f);
	TestEqual(TEXT("반경 0 은 중심만"), HBMagic::OrbExplosionFalloff(0.f, 0.f, 0.5f), 1.f);
	TestEqual(TEXT("가장자리 비율 범위 밖은 자름"), HBMagic::OrbExplosionFalloff(200.f, 200.f, 2.f), 1.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicDamageMultiplierTest, "HeavenBreach.Magic.DamageMultiplier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicDamageMultiplierTest::RunTest(const FString&)
{
	// 마법 스킬 피해 배율. 2배가 너무 세서 1.5배로 내렸다(2026-10-13).
	TestEqual(TEXT("마법 피해 배율 1.5"), HBMagic::DamageMultiplier, 1.5f);
	return true;
}
#endif
