// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveSwiftStep.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"

UGA_PassiveSwiftStep::UGA_PassiveSwiftStep()
{
	SelfPassiveEffect = UGE_SwiftStepPassive::StaticClass();

	// 단계 값은 그 단계의 "추가분" 최종값이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = ArcherTag::TAG_Augment_Passive_SwiftStep_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveSwiftStep", "AugmentName", "민첩한 발걸음 강화");
	Boost.Description = NSLOCTEXT("PassiveSwiftStep", "AugmentDesc", "이동속도 증가가 늘어납니다. (1단계 15%, 2단계 20%, 3단계 25%)");
	Boost.TierMagnitudes = { 0.05f, 0.10f, 0.15f };
	Boost.GrantEffect = UGE_SwiftStepAugment::StaticClass();
}
