// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveGlassCannon.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Magic/HB_MagicTag.h"

UGA_PassiveGlassCannon::UGA_PassiveGlassCannon()
{
	SelfPassiveEffect = UGE_GlassCannonPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MagicTag::TAG_Augment_Passive_GlassCannon_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveGlassCannon", "AugmentName", "유리 대포 강화");
	Boost.Description = NSLOCTEXT("PassiveGlassCannon", "AugmentDesc", "스킬 피해 증가가 늘어납니다. (1단계 35%, 2단계 45%, 3단계 60%)");
	Boost.TierMagnitudes = { 0.10f, 0.20f, 0.35f };
	Boost.GrantEffect = UGE_GlassCannonAugment::StaticClass();
}
