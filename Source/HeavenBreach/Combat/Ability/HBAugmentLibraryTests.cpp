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

namespace
{
	// 식별만 되면 되므로 CandidateId 만 채운다.
	FHBUpgradeCandidate MakeCandidate(const FString& Id)
	{
		FHBUpgradeCandidate C;
		C.CandidateId = FName(*Id);
		return C;
	}

	TArray<FHBUpgradeCandidate> MakePool(const FString& Prefix, const int32 Count)
	{
		TArray<FHBUpgradeCandidate> Pool;
		for (int32 I = 0; I < Count; ++I)
		{
			Pool.Add(MakeCandidate(FString::Printf(TEXT("%s%d"), *Prefix, I)));
		}
		return Pool;
	}

	int32 CountWithPrefix(const TArray<FHBUpgradeCandidate>& Out, const FString& Prefix)
	{
		int32 N = 0;
		for (const FHBUpgradeCandidate& C : Out)
		{
			if (C.CandidateId.ToString().StartsWith(Prefix))
			{
				++N;
			}
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBAugmentDrawTest, "HeavenBreach.Augment.Draw", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBAugmentDrawTest::RunTest(const FString&)
{
	FRandomStream Rng(12345);
	TArray<FHBUpgradeCandidate> Out;

	// 양쪽이 넉넉하면 3장, 공통은 상한 1장
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 5);
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 7);
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("3장을 뽑는다"), Out.Num(), 3);
		TestEqual(TEXT("공통은 1장만"), CountWithPrefix(Out, TEXT("C")), 1);
		TestEqual(TEXT("일반이 2장"), CountWithPrefix(Out, TEXT("P")), 2);
	}

	// 일반 풀이 모자라면 상한을 넘겨 공통으로 채운다
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 1);
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 7);
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("모자라도 3장을 채운다"), Out.Num(), 3);
		TestEqual(TEXT("일반은 있는 만큼"), CountWithPrefix(Out, TEXT("P")), 1);
		TestEqual(TEXT("공통이 상한을 넘겨 채운다"), CountWithPrefix(Out, TEXT("C")), 2);
	}

	// 일반 풀이 비었으면 전부 공통
	{
		TArray<FHBUpgradeCandidate> Pool;
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 7);
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("전부 공통으로 3장"), Out.Num(), 3);
		TestEqual(TEXT("공통 3장"), CountWithPrefix(Out, TEXT("C")), 3);
	}

	// 공통이 비었으면 전부 일반
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 5);
		TArray<FHBUpgradeCandidate> Common;
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("전부 일반으로 3장"), Out.Num(), 3);
		TestEqual(TEXT("일반 3장"), CountWithPrefix(Out, TEXT("P")), 3);
	}

	// 둘 다 합쳐 3장 미만이면 있는 만큼만
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 1);
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 1);
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("있는 만큼만 2장"), Out.Num(), 2);
	}

	// 둘 다 비면 0장
	{
		TArray<FHBUpgradeCandidate> Pool;
		TArray<FHBUpgradeCandidate> Common;
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TestEqual(TEXT("뽑을 게 없으면 0장"), Out.Num(), 0);
	}

	// 같은 후보가 두 번 나오지 않는다
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 3);
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 3);
		HBAugment::DrawCandidates(Pool, Common, 3, 1, Rng, Out);
		TSet<FName> Seen;
		for (const FHBUpgradeCandidate& C : Out)
		{
			TestFalse(TEXT("중복 없음"), Seen.Contains(C.CandidateId));
			Seen.Add(C.CandidateId);
		}
	}

	// 상한 0 이면 공통이 안 나온다. 단 일반이 모자라면 채우기는 여전히 동작
	{
		TArray<FHBUpgradeCandidate> Pool = MakePool(TEXT("P"), 5);
		TArray<FHBUpgradeCandidate> Common = MakePool(TEXT("C"), 5);
		HBAugment::DrawCandidates(Pool, Common, 3, 0, Rng, Out);
		TestEqual(TEXT("상한 0 이면 공통 0장"), CountWithPrefix(Out, TEXT("C")), 0);
		TestEqual(TEXT("일반으로 3장"), Out.Num(), 3);
	}

	// 같은 씨앗은 같은 결과를 준다
	{
		TArray<FHBUpgradeCandidate> PoolA = MakePool(TEXT("P"), 5);
		TArray<FHBUpgradeCandidate> CommonA = MakePool(TEXT("C"), 5);
		FRandomStream RngA(777);
		TArray<FHBUpgradeCandidate> OutA;
		HBAugment::DrawCandidates(PoolA, CommonA, 3, 1, RngA, OutA);

		TArray<FHBUpgradeCandidate> PoolB = MakePool(TEXT("P"), 5);
		TArray<FHBUpgradeCandidate> CommonB = MakePool(TEXT("C"), 5);
		FRandomStream RngB(777);
		TArray<FHBUpgradeCandidate> OutB;
		HBAugment::DrawCandidates(PoolB, CommonB, 3, 1, RngB, OutB);

		TestEqual(TEXT("같은 씨앗은 같은 장수"), OutA.Num(), OutB.Num());
		for (int32 I = 0; I < OutA.Num() && I < OutB.Num(); ++I)
		{
			TestEqual(TEXT("같은 씨앗은 같은 후보"), OutA[I].CandidateId, OutB[I].CandidateId);
		}
	}

	return true;
}

#endif
