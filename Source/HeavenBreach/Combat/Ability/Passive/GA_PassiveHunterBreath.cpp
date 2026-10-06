// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveHunterBreath.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"

UGA_PassiveHunterBreath::UGA_PassiveHunterBreath()
{
	SelfPassiveEffect = UGE_HunterBreathPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = ArcherTag::TAG_Augment_Passive_HunterBreath_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveHunterBreath", "AugmentName", "사냥꾼의 호흡 강화");
	Boost.Description = NSLOCTEXT("PassiveHunterBreath", "AugmentDesc", "달리기 스태미나 소모 감소가 늘어납니다. (1단계 40%, 2단계 50%, 3단계 60%)");
	Boost.TierMagnitudes = { -0.10f, -0.20f, -0.30f };
	Boost.GrantEffect = UGE_HunterBreathAugment::StaticClass();
}
