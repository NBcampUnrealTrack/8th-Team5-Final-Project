#include "Combat/Ability/HBAugmentLibrary.h"

namespace HBAugment
{
	int32 GetMaxTier(const FHBSkillAugment& Augment)
	{
		// 비어 있으면 단계 없는 1회성 증강이다. 0 이 아니라 1 이어야
		// 한 번은 먹을 수 있다.
		return FMath::Max(1, Augment.TierMagnitudes.Num());
	}

	int32 FindTier(const TArray<FHBAugmentStack>& Stacks, const FGameplayTag AugmentTag)
	{
		if (!AugmentTag.IsValid())
		{
			return 0;
		}
		for (const FHBAugmentStack& Stack : Stacks)
		{
			if (Stack.AugmentTag == AugmentTag)
			{
				return Stack.Tier;
			}
		}
		return 0;
	}

	bool RaiseTier(TArray<FHBAugmentStack>& Stacks, const FGameplayTag AugmentTag, const int32 MaxTier)
	{
		if (!AugmentTag.IsValid() || MaxTier <= 0)
		{
			return false;
		}

		for (FHBAugmentStack& Stack : Stacks)
		{
			if (Stack.AugmentTag != AugmentTag)
			{
				continue;
			}
			if (Stack.Tier >= MaxTier)
			{
				return false;   // 이미 최대다. 값을 건드리지 않는다
			}
			++Stack.Tier;
			return true;
		}

		// 목록에 없었다. 1단계로 넣는다
		FHBAugmentStack& Added = Stacks.AddDefaulted_GetRef();
		Added.AugmentTag = AugmentTag;
		Added.Tier = 1;
		return true;
	}

	float GetTierMagnitude(const FHBSkillAugment& Augment, const int32 Tier)
	{
		// 단계는 1 부터 센다. 배열은 0 부터다.
		const int32 Index = Tier - 1;
		return Augment.TierMagnitudes.IsValidIndex(Index) ? Augment.TierMagnitudes[Index] : 0.f;
	}

	void DrawCandidates(
		TArray<FHBUpgradeCandidate>& Pool,
		TArray<FHBUpgradeCandidate>& CommonPool,
		const int32 DrawCount,
		const int32 CommonCap,
		FRandomStream& Rng,
		TArray<FHBUpgradeCandidate>& Out)
	{
		Out.Reset();
		if (DrawCount <= 0)
		{
			return;
		}

		// 한 통에서 무작위로 한 장 꺼낸다. RemoveAtSwap 이라 O(1) 이다.
		auto TakeOne = [&Rng](TArray<FHBUpgradeCandidate>& From, TArray<FHBUpgradeCandidate>& Into)
		{
			const int32 Index = Rng.RandRange(0, From.Num() - 1);
			Into.Add(From[Index]);
			From.RemoveAtSwap(Index);
		};

		// 1. 공통에서 상한까지. Out 만으로는 어느 통에서 왔는지 알 수 없으므로
		// 카운터로 직접 센다.
		int32 CommonTaken = 0;
		while (Out.Num() < DrawCount && CommonPool.Num() > 0 && CommonTaken < CommonCap)
		{
			TakeOne(CommonPool, Out);
			++CommonTaken;
		}

		// 2. 나머지를 일반에서
		while (Out.Num() < DrawCount && Pool.Num() > 0)
		{
			TakeOne(Pool, Out);
		}

		// 3. 아직 모자라면 공통에서 더. 상한보다 장수를 채우는 것이 우선이다
		while (Out.Num() < DrawCount && CommonPool.Num() > 0)
		{
			TakeOne(CommonPool, Out);
		}
	}
}
