// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Melee/GA_MeleeBattleCry.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

UGA_MeleeBattleCry::UGA_MeleeBattleCry()
{
	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Efficiency = Augments.AddDefaulted_GetRef();
	Efficiency.AugmentTag = WarriorTag::TAG_Augment_BattleCry_Efficiency;
	Efficiency.DisplayName = NSLOCTEXT("MeleeBattleCry", "EfficiencyName", "전투의 함성: 효율");
	Efficiency.Description = NSLOCTEXT("MeleeBattleCry", "EfficiencyDesc", "전투의 함성의 재사용 대기시간과 마나 소모가 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_ManaCost, 0.85f);
	Efficiency.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	// 기본 지속 8초(BP) 기준 10/12/14초.
	FHBSkillAugment& Duration = Augments.AddDefaulted_GetRef();
	Duration.AugmentTag = WarriorTag::TAG_Augment_BattleCry_Duration;
	Duration.DisplayName = NSLOCTEXT("MeleeBattleCry", "DurationName", "전투의 함성: 지속");
	Duration.Description = NSLOCTEXT("MeleeBattleCry", "DurationDesc", "전투의 함성 지속시간이 늘어납니다. (1단계 25%, 2단계 50%, 3단계 75%)");
	Duration.Params.Add(SkillParamTag::TAG_SkillParam_Duration, 1.25f);
	Duration.TierMagnitudes = { 1.25f, 1.5f, 1.75f };

	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = WarriorTag::TAG_Augment_BattleCry_Damage;
	Power.DisplayName = NSLOCTEXT("MeleeBattleCry", "PowerName", "전투의 함성: 위력");
	Power.Description = NSLOCTEXT("MeleeBattleCry", "PowerDesc", "전투의 함성 동안 공격력이 오릅니다. (1단계 10%, 2단계 20%, 3단계 30%)");
	Power.TierMagnitudes = { 1.10f, 1.20f, 1.30f };
	Power.BuffEffect = UGE_WarriorBattleCryPowerAugment::StaticClass();
}
