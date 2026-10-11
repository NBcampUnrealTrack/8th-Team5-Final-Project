#pragma once
#include "CoreMinimal.h"

/** 근접 스킬 순수 계산. 상태를 갖지 않아 테스트하기 쉽다. */
namespace HBMelee
{
	/** 피의 갈증: 적 처치 시 최대 체력 대비 회복 비율. 없음 0 / 기본 0.05 / 강화 1·2·3단계 0.07·0.09·0.12. */
	inline float BloodthirstHealRatio(bool bHas, int32 BoostTier)
	{
		if (!bHas) return 0.f;
		switch (FMath::Min(BoostTier, 3))
		{
		case 1: return 0.07f;
		case 2: return 0.09f;
		case 3: return 0.12f;
		default: return 0.05f; // Tier <= 0 은 기본
		}
	}

	/** 견고한 자세: 홀드 방어 스태미나 소모 배율. 없음 1.0 / 기본 0.70 / 강화 1·2·3단계 0.60·0.50·0.40. */
	inline float SteadyStanceMultiplier(bool bHas, int32 BoostTier)
	{
		if (!bHas) return 1.f;
		switch (FMath::Min(BoostTier, 3))
		{
		case 1: return 0.60f;
		case 2: return 0.50f;
		case 3: return 0.40f;
		default: return 0.70f; // Tier <= 0 은 기본
		}
	}
}
