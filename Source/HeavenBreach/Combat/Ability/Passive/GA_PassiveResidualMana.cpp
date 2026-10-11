// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveResidualMana.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Magic/HB_MagicTag.h"

UGA_PassiveResidualMana::UGA_PassiveResidualMana()
{
	SelfPassiveEffect = UGE_ResidualManaPassive::StaticClass();

	// 단계 값은 그 단계의 최종값(쿨다운 감소 초)이다(누적 아님, 기본 1초를 대신한다). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MagicTag::TAG_Augment_Passive_ResidualMana_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveResidualMana", "AugmentName", "잔류 마력 강화");
	Boost.Description = NSLOCTEXT("PassiveResidualMana", "AugmentDesc", "쿨다운 감소가 늘어납니다. (1단계 1.5초, 2단계 2.0초, 3단계 2.5초)");
	Boost.TierMagnitudes = { 1.5f, 2.0f, 2.5f };
	// 쿨다운 감소 초(최종값). 코드가 단계를 읽어 쓴다(GE 없음).
}
