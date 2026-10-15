// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Bow/GA_BowTripleShot.h"

#include "Combat/Attack/HBBowShot.h"
#include "Combat/Attack/HBPlayerArrow.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Player/HBPlayerCharacter.h"

UGA_BowTripleShot::UGA_BowTripleShot()
{
	// 부모(관통 사격) 생성자 값 중 다른 것만 바꾼다. 준비·당김·발사 시간은 같은 몽타주 구조라 그대로 쓴다.
	Cooldown = 8.f;
	ManaCost = 25.f;
	DamageCoef = 0.8f;
	MaxPierceCount = 0;
	CooldownGameplayEffectClass = UGE_BowTripleShotCooldown::StaticClass();

	// 부모가 넣은 관통 사격 증강을 비우고 트리플 샷 증강으로 바꾼다.
	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	Augments.Reset();

	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = ArcherTag::TAG_Augment_Bow_TripleShot_Power;
	Power.DisplayName = NSLOCTEXT("BowTripleShot", "PowerName", "트리플 샷: 위력");
	Power.Description = NSLOCTEXT("BowTripleShot", "PowerDesc", "트리플 샷 화살 피해가 늘어납니다. (1단계 15%, 2단계 30%, 3단계 50%)");
	Power.Params.Add(SkillParamTag::TAG_SkillParam_Damage, 1.15f);
	Power.TierMagnitudes = { 1.15f, 1.30f, 1.50f };

	FHBSkillAugment& Rapid = Augments.AddDefaulted_GetRef();
	Rapid.AugmentTag = ArcherTag::TAG_Augment_Bow_TripleShot_Rapid;
	Rapid.DisplayName = NSLOCTEXT("BowTripleShot", "RapidName", "트리플 샷: 속사");
	Rapid.Description = NSLOCTEXT("BowTripleShot", "RapidDesc", "트리플 샷 재사용 대기시간이 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Rapid.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Rapid.TierMagnitudes = { 0.85f, 0.75f, 0.65f };
}

void UGA_BowTripleShot::FireSkillArrows(AHBPlayerCharacter* Player, const TSubclassOf<AHBPlayerArrow> ArrowClassToUse, const FDataTableRowHandle& ArrowRow,
	const float FinalCoef, const FHBArrowLaunchOptions& Options)
{
	// 가운데가 조준점. 화살마다 맞으면 각자 피해·박힘을 처리한다.
	int32 Spawned = 0;
	for (const float Yaw : HBBowShot::SpreadYawOffsets(ArrowCount, SpreadDegrees))
	{
		if (HBBowShot::SpawnArrow(Player, ArrowClassToUse, ArrowSpeed, FinalCoef, ArrowRow, AimTraceDistance, SpawnForwardOffset, Options, Yaw))
		{
			++Spawned;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 트리플 샷: 발사  화살=%d/%d  Coef=%.2f  간격=%.1f도"), Spawned, ArrowCount, FinalCoef, SpreadDegrees);
}
