#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

namespace HBDamage
{
	inline bool IsHeadshot(const FHitResult& Hit)
	{
		return Hit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
	}

	inline float CalculateFinalDamage(
		const float RawDamage,
		const float TargetDefense,
		const float HitMultiplier,
		const float BlockMultiplier = 1.f,
		const float AdditionalMultiplier = 1.f)
	{
		if (RawDamage <= 0.f)
		{
			return 0.f;
		}

		const float SafeDefense = FMath::Max(0.f, TargetDefense);
		const float DefenseMultiplier = 100.f / (100.f + SafeDefense);
		const float FinalDamage =
			RawDamage
			* FMath::Max(0.f, HitMultiplier)
			* DefenseMultiplier
			* FMath::Clamp(BlockMultiplier, 0.f, 1.f)
			* FMath::Max(0.f, AdditionalMultiplier);

		return FMath::Max(1.f, FinalDamage);
	}
}
