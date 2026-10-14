// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HBAttributeRules
{
	/**
	 * 최대 체력이 바뀔 때 현재 체력. InOutHighestMax 는 지금까지 도달한 가장 높은 최대 체력(갱신된다).
	 * - 최고치를 넘어 오르면 넘은 만큼 현재 체력도 채운다(증강·패시브로 오르면 바로 차오른다).
	 * - 최고치 이하로 다시 오르는 것(장비를 벗었다 다시 낌)은 채우지 않는다. 벗고 끼기로 무한 회복이 되지 않게.
	 * - 내리면 넘치는 만큼만 자른다. 죽어 있으면(0) 그대로.
	 */
	inline float HealthOnMaxChanged(float Current, float OldMax, float NewMax, float& InOutHighestMax)
	{
		if (Current <= 0.f)
		{
			InOutHighestMax = FMath::Max(InOutHighestMax, NewMax);
			return Current;
		}

		const float Gain = FMath::Max(0.f, NewMax - FMath::Max(OldMax, InOutHighestMax));
		InOutHighestMax = FMath::Max(InOutHighestMax, NewMax);
		return FMath::Clamp(Current + Gain, 0.f, NewMax);
	}
}
