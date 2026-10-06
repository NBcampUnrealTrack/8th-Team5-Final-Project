#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"

/**
 * 사냥꾼의 표식이 대상에게 주는 받는 피해 증가율 계산. 액터도 월드도 쓰지 않는 순수 함수다.
 * 기본 표식 +20%, 깊은 표식 1/2/3단계 +25/30/35%. 겹치면 가장 큰 값 하나만 쓴다.
 */
namespace HBHunterMark
{
	// 대상의 태그에서 받는 피해 증가율을 구한다. 표식이 없으면 0.
	inline float DamageBonus(const FGameplayTagContainer& TargetTags)
	{
		if (TargetTags.HasTagExact(ArcherTag::TAG_State_Tracked_Deep3)) { return 0.35f; }
		if (TargetTags.HasTagExact(ArcherTag::TAG_State_Tracked_Deep2)) { return 0.30f; }
		if (TargetTags.HasTagExact(ArcherTag::TAG_State_Tracked_Deep1)) { return 0.25f; }
		if (TargetTags.HasTagExact(ArcherTag::TAG_State_Tracked)) { return 0.20f; }
		return 0.f;
	}

	// 증가율에 맞는 깊은 표식 태그. 0.25 미만(기본 표식)은 무효 태그를 준다.
	inline FGameplayTag TagForBonus(const float Bonus)
	{
		if (Bonus >= 0.35f - KINDA_SMALL_NUMBER) { return ArcherTag::TAG_State_Tracked_Deep3; }
		if (Bonus >= 0.30f - KINDA_SMALL_NUMBER) { return ArcherTag::TAG_State_Tracked_Deep2; }
		if (Bonus >= 0.25f - KINDA_SMALL_NUMBER) { return ArcherTag::TAG_State_Tracked_Deep1; }
		return FGameplayTag();
	}
}
