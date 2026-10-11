// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveBloodthirst.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"

UGA_PassiveBloodthirst::UGA_PassiveBloodthirst()
{
	SelfPassiveEffect = UGE_BloodthirstPassive::StaticClass();

	// 단계 값은 그 단계의 최종 회복 비율이다(누적 아님). 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MeleeTag::TAG_Augment_Passive_Bloodthirst_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveBloodthirst", "AugmentName", "피의 갈증 강화");
	Boost.Description = NSLOCTEXT("PassiveBloodthirst", "AugmentDesc", "처치 시 회복이 늘어납니다. (기본 최대 체력 5%에서 1단계 7%, 2단계 9%, 3단계 12%)");
	Boost.TierMagnitudes = { 0.07f, 0.09f, 0.12f };
	// 회복 비율은 코드가 단계를 읽어 정한다(GE 없음). TierMagnitudes 는 설명용 최종 비율이다.
}
