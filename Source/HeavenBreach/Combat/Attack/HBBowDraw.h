// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 활 당김 튜닝 값. GA 의 EditDefaultsOnly 값을 모아 넘긴다 */
struct FHBBowDrawParams
{
	float MinDrawTime = 0.3f;
	float MaxDrawTime = 1.0f;
	float MinArrowSpeed = 1500.f;
	float MaxArrowSpeed = 4000.f;
	float MinDamageCoef = 0.5f;
	float MaxDamageCoef = 1.0f;
};

/** 떼는 순간의 결과. bFire 가 false 면 나머지는 의미 없다 */
struct FHBBowShot
{
	bool bFire = false;
	float Speed = 0.f;
	float DamageCoef = 0.f;
};

namespace HBBow
{
	/** 누른 시간으로 발사 여부·속도·계수를 정한다. 최소 미만은 취소, 최소~최대는 선형, 넘으면 최대 */
	inline FHBBowShot ComputeShot(const float TimeHeld, const FHBBowDrawParams& Params)
	{
		FHBBowShot Shot;
		if (TimeHeld < Params.MinDrawTime)
		{
			return Shot;
		}

		const float Range = Params.MaxDrawTime - Params.MinDrawTime;
		const float Charge = Range > KINDA_SMALL_NUMBER
			? FMath::Clamp((TimeHeld - Params.MinDrawTime) / Range, 0.f, 1.f)
			: 1.f;

		Shot.bFire = true;
		Shot.Speed = FMath::Lerp(Params.MinArrowSpeed, Params.MaxArrowSpeed, Charge);
		Shot.DamageCoef = FMath::Lerp(Params.MinDamageCoef, Params.MaxDamageCoef, Charge);
		return Shot;
	}
}
