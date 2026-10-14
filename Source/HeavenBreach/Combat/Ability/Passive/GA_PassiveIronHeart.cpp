// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveIronHeart.h"

#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"

UGA_PassiveIronHeart::UGA_PassiveIronHeart()
{
	// 단계 값은 그 단계의 "추가" 배율이다(누적 아님). 곱셈 모디파이어는 (배율 - 1) 끼리 더해지므로
	// 패시브 ×1.5 와 합쳐 ×1.6/1.7/1.85 가 된다. 설명은 숫자를 글에 적으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Boost = Augments.AddDefaulted_GetRef();
	Boost.AugmentTag = MeleeTag::TAG_Augment_Passive_IronHeart_Boost;
	Boost.DisplayName = NSLOCTEXT("PassiveIronHeart", "AugmentName", "강철 심장 강화");
	Boost.Description = NSLOCTEXT("PassiveIronHeart", "AugmentDesc", "최대 체력 증가가 늘어납니다. (기본 50%에서 1단계 60%, 2단계 70%, 3단계 85%)");
	Boost.TierMagnitudes = { 1.10f, 1.20f, 1.35f };
	Boost.GrantEffect = UGE_IronHeartAugment::StaticClass();
}
