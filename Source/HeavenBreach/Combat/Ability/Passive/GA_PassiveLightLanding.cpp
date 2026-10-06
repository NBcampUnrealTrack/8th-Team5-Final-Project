// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveLightLanding.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"

UGA_PassiveLightLanding::UGA_PassiveLightLanding()
{
	SelfPassiveEffect = UGE_LightLandingPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 패시브 0.4 와 더해 55/65/75%.
	// 설명 치환은 Params 만 지원해서 숫자를 글에 적는다. 수치를 바꾸면 글도 같이 고칠 것.
	FHBSkillAugment& Reduction = Augments.AddDefaulted_GetRef();
	Reduction.AugmentTag = AbilityTag::TAG_Augment_Passive_LightLanding_Reduction;
	Reduction.DisplayName = NSLOCTEXT("PassiveLightLanding", "AugmentName", "가벼운 착지 강화");
	Reduction.Description = NSLOCTEXT("PassiveLightLanding", "AugmentDesc", "낙하 피해 감소가 늘어납니다. (1단계 55%, 2단계 65%, 3단계 75%)");
	Reduction.TierMagnitudes = { 0.15f, 0.25f, 0.35f };
	Reduction.GrantEffect = UGE_LightLandingAugment::StaticClass();
}
