// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveManaCycle.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Magic/HB_MagicTag.h"

UGA_PassiveManaCycle::UGA_PassiveManaCycle()
{
	SelfPassiveEffect = UGE_ManaCyclePassive::StaticClass();

	// 단계 값은 그 단계의 최종 배율이다(누적 아님, 기본 0.80 을 대신한다). 코드가 단계를 읽어 쓴다(GE 없음, HBMagic::ManaCycleMultiplier).
	// 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MagicTag::TAG_Augment_Passive_ManaCycle_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveManaCycle", "AugmentName", "마력 순환 강화");
	Boost.Description = NSLOCTEXT("PassiveManaCycle", "AugmentDesc", "액티브 스킬 마나 소모 감소가 늘어납니다. (합계 1단계 30%, 2단계 40%, 3단계 50%)");
	Boost.TierMagnitudes = { 0.70f, 0.60f, 0.50f };
}
