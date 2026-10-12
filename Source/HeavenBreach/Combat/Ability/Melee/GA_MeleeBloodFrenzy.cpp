// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Melee/GA_MeleeBloodFrenzy.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

UGA_MeleeBloodFrenzy::UGA_MeleeBloodFrenzy()
{
	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Efficiency = Augments.AddDefaulted_GetRef();
	Efficiency.AugmentTag = WarriorTag::TAG_Augment_BloodFrenzy_Efficiency;
	Efficiency.DisplayName = NSLOCTEXT("MeleeBloodFrenzy", "EfficiencyName", "피의 광란: 효율");
	Efficiency.Description = NSLOCTEXT("MeleeBloodFrenzy", "EfficiencyDesc", "피의 광란의 재사용 대기시간과 마나 소모가 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_ManaCost, 0.85f);
	Efficiency.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	// 기본 흡혈 20%(BP 버프 GE)에 더한다. 합계 25/30/40%.
	FHBSkillAugment& Lifesteal = Augments.AddDefaulted_GetRef();
	Lifesteal.AugmentTag = WarriorTag::TAG_Augment_BloodFrenzy_Lifesteal;
	Lifesteal.DisplayName = NSLOCTEXT("MeleeBloodFrenzy", "LifestealName", "피의 광란: 흡혈");
	Lifesteal.Description = NSLOCTEXT("MeleeBloodFrenzy", "LifestealDesc", "피의 광란의 흡혈량이 늘어납니다. (기본 20%에서 1단계 25%, 2단계 30%, 3단계 40%)");
	Lifesteal.TierMagnitudes = { 0.05f, 0.10f, 0.20f };
	Lifesteal.BuffEffect = UGE_WarriorBloodFrenzyLifestealAugment::StaticClass();

	// 기본 지속 8초(BP) 기준 10/12/14초.
	FHBSkillAugment& Duration = Augments.AddDefaulted_GetRef();
	Duration.AugmentTag = WarriorTag::TAG_Augment_BloodFrenzy_Duration;
	Duration.DisplayName = NSLOCTEXT("MeleeBloodFrenzy", "DurationName", "피의 광란: 지속");
	Duration.Description = NSLOCTEXT("MeleeBloodFrenzy", "DurationDesc", "피의 광란 지속시간이 늘어납니다. (1단계 25%, 2단계 50%, 3단계 75%)");
	Duration.Params.Add(SkillParamTag::TAG_SkillParam_Duration, 1.25f);
	Duration.TierMagnitudes = { 1.25f, 1.5f, 1.75f };
}
