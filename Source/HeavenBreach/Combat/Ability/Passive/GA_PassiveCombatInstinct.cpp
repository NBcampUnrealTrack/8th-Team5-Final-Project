// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveCombatInstinct.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"

UGA_PassiveCombatInstinct::UGA_PassiveCombatInstinct()
{
	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MeleeTag::TAG_Augment_Passive_CombatInstinct_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveCombatInstinct", "AugmentName", "전투 본능 강화");
	Boost.Description = NSLOCTEXT("PassiveCombatInstinct", "AugmentDesc", "체력이 낮을 때 오르는 최대 공격력이 늘어납니다. (기본 50%에서 1단계 60%, 2단계 70%, 3단계 80%)");
	Boost.TierMagnitudes = { 0.10f, 0.20f, 0.30f };
	Boost.GrantEffect = UGE_CombatInstinctAugment::StaticClass();
}
