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

	bool ResolveParam(const FHBSkillAugment& Augment, const FGameplayTag ParamTag, const int32 Tier, float& OutValue)
	{
		float ParamValue = 0.f;
		if (!Augment.FindParam(ParamTag, ParamValue))
		{
			return false;   // 이 증강은 그 항목을 안 정했다
		}
		OutValue = Augment.TierMagnitudes.Num() > 0 ? GetTierMagnitude(Augment, Tier) : ParamValue;
		return true;
	}

	void DrawCandidates(
		TArray<FHBUpgradeCandidate>& Pool,
		const int32 DrawCount,
		FRandomStream& Rng,
		TArray<FHBUpgradeCandidate>& Out)
	{
		Out.Reset();

		// 무작위로 한 장씩 꺼낸다. RemoveAtSwap 이라 O(1) 이다.
		while (Out.Num() < DrawCount && Pool.Num() > 0)
		{
			const int32 Index = Rng.RandRange(0, Pool.Num() - 1);
			Out.Add(Pool[Index]);
			Pool.RemoveAtSwap(Index);
		}
	}
}
