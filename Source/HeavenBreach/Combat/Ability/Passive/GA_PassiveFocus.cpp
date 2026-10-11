// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveFocus.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Magic/HB_MagicTag.h"

UGA_PassiveFocus::UGA_PassiveFocus()
{
	SelfPassiveEffect = UGE_FocusPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MagicTag::TAG_Augment_Passive_Focus_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveFocus", "AugmentName", "집중 강화");
	Boost.Description = NSLOCTEXT("PassiveFocus", "AugmentDesc", "시전 시간이 더 줄어듭니다. (기본 25% 단축에서 1단계 35%, 2단계 45%, 3단계 55% 단축)");
	Boost.TierMagnitudes = { 0.65f, 0.55f, 0.45f };
	// 시전 시간 배율은 코드가 단계를 읽어 정한다(GE 없음). TierMagnitudes 는 설명용 최종 배율이다.
}
