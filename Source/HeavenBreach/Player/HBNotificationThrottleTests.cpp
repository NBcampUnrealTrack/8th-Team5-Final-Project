#include "Player/HBNotificationThrottle.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBNotifyThrottleTest, "HeavenBreach.Notify.Throttle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBNotifyThrottleTest::RunTest(const FString&)
{
	const FText A = FText::FromString(TEXT("화살이 없습니다"));
	const FText B = FText::FromString(TEXT("가방이 가득 찼습니다"));

	TestTrue(TEXT("처음 띄우는 문구는 보여 준다"), HBNotify::ShouldShow(FText::GetEmpty(), -1.0e9, A, 10.0, 1.5));
	TestFalse(TEXT("같은 문구를 1.5초 안에 다시 띄우지 않는다"), HBNotify::ShouldShow(A, 10.0, A, 11.0, 1.5));
	TestTrue(TEXT("같은 문구라도 1.5초가 지나면 보여 준다"), HBNotify::ShouldShow(A, 10.0, A, 11.5, 1.5));
	TestTrue(TEXT("다른 문구는 바로 보여 준다"), HBNotify::ShouldShow(A, 10.0, B, 10.1, 1.5));
	TestFalse(TEXT("빈 문구는 띄우지 않는다"), HBNotify::ShouldShow(A, 10.0, FText::GetEmpty(), 20.0, 1.5));

	return true;
}

#endif
