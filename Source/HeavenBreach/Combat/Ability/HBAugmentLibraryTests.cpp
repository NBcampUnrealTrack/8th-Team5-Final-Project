#include "Combat/Ability/HBAugmentLibrary.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameplayTag TierTestTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Augment.Warrior.StrongBody.Defense"));
	}

	FGameplayTag TierOtherTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Augment.Warrior.CombatBreath.Free"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBAugmentTierTest, "HeavenBreach.Augment.Tier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBAugmentTierTest::RunTest(const FString&)
{
	// GetMaxTier
	FHBSkillAugment Empty;
	TestEqual(TEXT("TierMagnitudes 가 비면 최대 단계는 1"), HBAugment::GetMaxTier(Empty), 1);

	FHBSkillAugment Three;
	Three.TierMagnitudes = { 10.f, 15.f, 25.f };
	TestEqual(TEXT("3개면 최대 단계는 3"), HBAugment::GetMaxTier(Three), 3);

	// FindTier
	TArray<FHBAugmentStack> Stacks;
	TestEqual(TEXT("빈 목록에서는 0단계"), HBAugment::FindTier(Stacks, TierTestTag()), 0);

	// RaiseTier 누적
	TestTrue(TEXT("첫 획득은 성공"), HBAugment::RaiseTier(Stacks, TierTestTag(), 3));
	TestEqual(TEXT("첫 획득 후 1단계"), HBAugment::FindTier(Stacks, TierTestTag()), 1);
	TestEqual(TEXT("항목이 하나 생겼다"), Stacks.Num(), 1);

	TestTrue(TEXT("2단계 성공"), HBAugment::RaiseTier(Stacks, TierTestTag(), 3));
	TestTrue(TEXT("3단계 성공"), HBAugment::RaiseTier(Stacks, TierTestTag(), 3));
	TestEqual(TEXT("3단계 도달"), HBAugment::FindTier(Stacks, TierTestTag()), 3);
	TestEqual(TEXT("항목은 여전히 하나"), Stacks.Num(), 1);

	// 최대 초과는 거부하고 값을 바꾸지 않는다
	TestFalse(TEXT("최대를 넘기면 실패"), HBAugment::RaiseTier(Stacks, TierTestTag(), 3));
	TestEqual(TEXT("실패했으면 단계가 그대로"), HBAugment::FindTier(Stacks, TierTestTag()), 3);

	// 증강끼리 독립
	TestEqual(TEXT("다른 증강은 0단계"), HBAugment::FindTier(Stacks, TierOtherTag()), 0);
	TestTrue(TEXT("다른 증강도 올라간다"), HBAugment::RaiseTier(Stacks, TierOtherTag(), 1));
	TestEqual(TEXT("이제 항목 둘"), Stacks.Num(), 2);
	TestEqual(TEXT("먼저 것은 영향 없음"), HBAugment::FindTier(Stacks, TierTestTag()), 3);
	TestFalse(TEXT("최대 1단계 증강은 두 번 못 먹는다"), HBAugment::RaiseTier(Stacks, TierOtherTag(), 1));

	// GetTierMagnitude 는 1 부터 센다
	TestEqual(TEXT("1단계 수치"), HBAugment::GetTierMagnitude(Three, 1), 10.f);
	TestEqual(TEXT("2단계 수치"), HBAugment::GetTierMagnitude(Three, 2), 15.f);
	TestEqual(TEXT("3단계 수치"), HBAugment::GetTierMagnitude(Three, 3), 25.f);
	TestEqual(TEXT("0단계는 0"), HBAugment::GetTierMagnitude(Three, 0), 0.f);
	TestEqual(TEXT("범위 초과는 0"), HBAugment::GetTierMagnitude(Three, 4), 0.f);
	TestEqual(TEXT("빈 배열은 0"), HBAugment::GetTierMagnitude(Empty, 1), 0.f);

	// 유효하지 않은 태그는 거부한다
	TArray<FHBAugmentStack> Fresh;
	TestFalse(TEXT("빈 태그는 올릴 수 없다"), HBAugment::RaiseTier(Fresh, FGameplayTag(), 3));
	TestEqual(TEXT("빈 태그로는 항목이 안 생긴다"), Fresh.Num(), 0);

	// MaxTier 가 0 이하면 태그가 유효해도 거부한다
	TArray<FHBAugmentStack> ZeroMax;
	TestFalse(TEXT("MaxTier 0 은 유효한 태그도 거부한다"), HBAugment::RaiseTier(ZeroMax, TierTestTag(), 0));
	TestEqual(TEXT("MaxTier 0 이면 항목이 안 생긴다"), ZeroMax.Num(), 0);

	TArray<FHBAugmentStack> NegativeMax;
	TestFalse(TEXT("MaxTier 음수도 유효한 태그를 거부한다"), HBAugment::RaiseTier(NegativeMax, TierTestTag(), -1));
	TestEqual(TEXT("MaxTier 음수면 항목이 안 생긴다"), NegativeMax.Num(), 0);

	return true;
}

#endif
