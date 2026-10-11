// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveSteadyStance.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"

UGA_PassiveSteadyStance::UGA_PassiveSteadyStance()
{
	SelfPassiveEffect = UGE_SteadyStancePassive::StaticClass();

	// 단계 값은 그 단계의 최종 소모 배율이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MeleeTag::TAG_Augment_Passive_SteadyStance_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveSteadyStance", "AugmentName", "견고한 자세 강화");
	Boost.Description = NSLOCTEXT("PassiveSteadyStance", "AugmentDesc", "홀드 방어 스태미나 소모가 더 줄어듭니다. (기본 30% 감소에서 1단계 40%, 2단계 50%, 3단계 60% 감소)");
	Boost.TierMagnitudes = { 0.60f, 0.50f, 0.40f };
	// 소모 배율은 코드가 단계를 읽어 정한다(GE 없음). TierMagnitudes 는 설명용 최종 배율이다.
}
