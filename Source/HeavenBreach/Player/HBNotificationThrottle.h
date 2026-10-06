// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HBNotify
{
	/**
	 * 알림을 지금 띄울지. HUD 알림은 큐에 쌓이므로, 같은 문구를 연타로 넣으면 줄줄이 밀린다.
	 * 직전과 같은 문구가 Interval 초 안에 다시 오면 버린다. 빈 문구는 띄우지 않는다.
	 */
	inline bool ShouldShow(const FText& LastText, const double LastTime, const FText& NewText, const double Now, const double Interval)
	{
		if (NewText.IsEmpty())
		{
			return false;
		}
		if (!LastText.EqualTo(NewText))
		{
			return true;
		}
		return (Now - LastTime) >= Interval;
	}
}
