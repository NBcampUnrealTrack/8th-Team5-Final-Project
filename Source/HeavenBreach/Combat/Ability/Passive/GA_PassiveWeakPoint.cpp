// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveWeakPoint.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"

UGA_PassiveWeakPoint::UGA_PassiveWeakPoint()
{
	SelfPassiveEffect = UGE_WeakPointPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = ArcherTag::TAG_Augment_Passive_WeakPoint_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveWeakPoint", "AugmentName", "급소 포착 강화");
	Boost.Description = NSLOCTEXT("PassiveWeakPoint", "AugmentDesc", "헤드샷 피해 증가가 늘어납니다. (1단계 45%, 2단계 60%, 3단계 80%)");
	Boost.TierMagnitudes = { 0.15f, 0.30f, 0.50f };
	Boost.GrantEffect = UGE_WeakPointAugment::StaticClass();
}
