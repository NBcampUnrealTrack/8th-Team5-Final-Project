#include "Combat/Attack/HBBowDraw.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowComputeShotTest, "HeavenBreach.Bow.ComputeShot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowComputeShotTest::RunTest(const FString&)
{
	FHBBowDrawParams P;
	P.MinDrawTime = 0.3f;
	P.MaxDrawTime = 1.0f;
	P.MinArrowSpeed = 1500.f;
	P.MaxArrowSpeed = 4000.f;
	P.MinDamageCoef = 0.5f;
	P.MaxDamageCoef = 1.0f;

	TestFalse(TEXT("최소 당김 미만이면 발사 안 함"), HBBow::ComputeShot(0.29f, P).bFire);

	const FHBBowShot AtMin = HBBow::ComputeShot(0.3f, P);
	TestTrue(TEXT("최소 당김이면 발사"), AtMin.bFire);
	TestEqual(TEXT("최소 당김 속도"), AtMin.Speed, 1500.f);
	TestEqual(TEXT("최소 당김 계수"), AtMin.DamageCoef, 0.5f);

	const FHBBowShot Half = HBBow::ComputeShot(0.65f, P);
	TestEqual(TEXT("중간 속도"), Half.Speed, 2750.f, 0.01f);
	TestEqual(TEXT("중간 계수"), Half.DamageCoef, 0.75f, 0.0001f);

	const FHBBowShot Over = HBBow::ComputeShot(5.f, P);
	TestEqual(TEXT("최대 넘어가면 최대 속도"), Over.Speed, 4000.f);
	TestEqual(TEXT("최대 넘어가면 최대 계수"), Over.DamageCoef, 1.0f);

	// Max <= Min 으로 잘못 넣어도 0 나누기 없이 최대로 쏜다.
	FHBBowDrawParams Bad = P;
	Bad.MaxDrawTime = 0.3f;
	const FHBBowShot BadShot = HBBow::ComputeShot(0.3f, Bad);
	TestTrue(TEXT("잘못된 범위도 발사"), BadShot.bFire);
	TestEqual(TEXT("잘못된 범위는 최대 계수"), BadShot.DamageCoef, 1.0f);

	return true;
}

#endif
