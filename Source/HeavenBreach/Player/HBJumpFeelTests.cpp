#include "Player/HBJumpFeel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBJumpFeelImpactTest, "HeavenBreach.JumpFeel.LandingImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBJumpFeelImpactTest::RunTest(const FString&)
{
	TestEqual(TEXT("부드러운 속도 이하는 0"), HBJumpFeel::LandingImpact(300.f, 450.f, 1100.f), 0.f);
	TestEqual(TEXT("부드러운 속도 딱 0"), HBJumpFeel::LandingImpact(450.f, 450.f, 1100.f), 0.f);
	TestEqual(TEXT("중간은 비례"), HBJumpFeel::LandingImpact(775.f, 450.f, 1100.f), 0.5f, 0.001f);
	TestEqual(TEXT("강한 속도 이상은 1"), HBJumpFeel::LandingImpact(2000.f, 450.f, 1100.f), 1.f);
	TestEqual(TEXT("범위가 잘못되면 Soft 넘으면 1"), HBJumpFeel::LandingImpact(500.f, 450.f, 400.f), 1.f);
	TestEqual(TEXT("범위가 잘못돼도 Soft 이하는 0"), HBJumpFeel::LandingImpact(400.f, 450.f, 400.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBJumpFeelSpeedTest, "HeavenBreach.JumpFeel.LandingSpeedScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBJumpFeelSpeedTest::RunTest(const FString&)
{
	TestEqual(TEXT("착지 순간 최대 감속"), HBJumpFeel::LandingSpeedScale(1.f, 0.4f, 0.f, 0.35f), 0.6f, 0.001f);
	TestEqual(TEXT("충격 절반이면 감속 절반"), HBJumpFeel::LandingSpeedScale(0.5f, 0.4f, 0.f, 0.35f), 0.8f, 0.001f);
	TestEqual(TEXT("절반 시간이면 절반 회복"), HBJumpFeel::LandingSpeedScale(1.f, 0.4f, 0.175f, 0.35f), 0.8f, 0.001f);
	TestEqual(TEXT("시간 다 지나면 1"), HBJumpFeel::LandingSpeedScale(1.f, 0.4f, 0.35f, 0.35f), 1.f);
	TestEqual(TEXT("한참 지나도 1"), HBJumpFeel::LandingSpeedScale(1.f, 0.4f, 5.f, 0.35f), 1.f);
	TestEqual(TEXT("충격 0 이면 1"), HBJumpFeel::LandingSpeedScale(0.f, 0.4f, 0.f, 0.35f), 1.f);
	TestEqual(TEXT("시간 0 설정이면 바로 1"), HBJumpFeel::LandingSpeedScale(1.f, 0.4f, 0.f, 0.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBJumpFeelRejumpTest, "HeavenBreach.JumpFeel.JumpHeightScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBJumpFeelRejumpTest::RunTest(const FString&)
{
	TestEqual(TEXT("창 안이면 줄어듦"), HBJumpFeel::JumpHeightScale(0.1f, 0.3f, 0.75f), 0.75f);
	TestEqual(TEXT("창 끝나면 정상"), HBJumpFeel::JumpHeightScale(0.3f, 0.3f, 0.75f), 1.f);
	TestEqual(TEXT("한 번도 착지 안 했으면(큰 값) 정상"), HBJumpFeel::JumpHeightScale(1000.f, 0.3f, 0.75f), 1.f);
	return true;
}

#endif
