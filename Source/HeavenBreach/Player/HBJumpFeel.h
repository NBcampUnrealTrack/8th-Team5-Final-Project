// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 점프 감각 계산. 서버와 소유 클라가 같은 입력으로 같은 값을 내야 이동 예측이 어긋나지 않아서
 * 상태 없이 순수 함수로 둔다. 설계: Docs/점프-감각-설계.md
 */
namespace HBJumpFeel
{
	/** 착지 직전 하강 속도 → 충격 0~1. Soft 이하 0, Hard 이상 1, 사이는 선형 */
	inline float LandingImpact(const float DownSpeed, const float SoftSpeed, const float HardSpeed)
	{
		if (DownSpeed <= SoftSpeed)
		{
			return 0.f;
		}
		const float Range = HardSpeed - SoftSpeed;
		if (Range <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}
		return FMath::Clamp((DownSpeed - SoftSpeed) / Range, 0.f, 1.f);
	}

	/** 착지 후 이동 속도 배율. 착지 순간 1 - MaxSlow×Impact 에서 Duration 동안 선형으로 1 까지 회복 */
	inline float LandingSpeedScale(const float Impact, const float MaxSlow, const float Elapsed, const float Duration)
	{
		if (Duration <= KINDA_SMALL_NUMBER || Elapsed >= Duration)
		{
			return 1.f;
		}
		const float Lowest = 1.f - FMath::Clamp(MaxSlow * Impact, 0.f, 1.f);
		const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
		return FMath::Lerp(Lowest, 1.f, Alpha);
	}

	/** 착지 후 Window 안에 다시 뛰면 ReducedScale, 아니면 1 */
	inline float JumpHeightScale(const float SinceLanded, const float Window, const float ReducedScale)
	{
		return SinceLanded < Window ? ReducedScale : 1.f;
	}
}
