// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 낙하 피해 계산. 캐릭터와 분리해 테스트한다 */
namespace HBFallDamage
{
	/** 낙하 높이(cm)를 피해 비율 0~1 로. 안전 높이 이하 0, 치명 높이 이상 1, 사이는 직선 */
	inline float DamageRatio(const float FallHeight, const float SafeHeight, const float LethalHeight)
	{
		if (FallHeight <= SafeHeight)
		{
			return 0.f;
		}
		if (LethalHeight <= SafeHeight)
		{
			// 범위를 잘못 넣어도 0 나누기 없이, 안전 높이를 넘으면 치명으로 본다.
			return 1.f;
		}
		return FMath::Clamp((FallHeight - SafeHeight) / (LethalHeight - SafeHeight), 0.f, 1.f);
	}

	/** 최종 피해. 감소는 0~MaxReduction 으로 묶는다(감소 수단을 다 겹쳐도 무적이 되지 않게) */
	inline float ComputeDamage(const float MaxHP, const float FallHeight, const float SafeHeight, const float LethalHeight, const float Reduction, const float MaxReduction)
	{
		const float Ratio = DamageRatio(FallHeight, SafeHeight, LethalHeight);
		const float ClampedReduction = FMath::Clamp(Reduction, 0.f, FMath::Clamp(MaxReduction, 0.f, 1.f));
		return FMath::Max(0.f, MaxHP) * Ratio * (1.f - ClampedReduction);
	}
}
