#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;

namespace HBDamage
{
	inline bool IsHeadshot(const FHitResult& Hit)
	{
		return Hit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
	}

	// 때린 사람의 헤드샷 추가 피해 (0.1 = 10%). 플레이어가 아니면 0.
	// 적이 맞을 때와 플레이어가 맞을 때(PvP) 두 곳이 같이 쓴다.
	//
	// 왜 맞는 쪽이 읽나: 부위 배율은 맞는 쪽이 정한다 (BoneDamageMultipliers).
	// 때리는 쪽은 뼈 이름을 모르고 넘기기만 해서, 머리인지는 맞는 쪽에서만 안다.
	float GetAttackerHeadshotBonus(const AActor* Attacker);

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
