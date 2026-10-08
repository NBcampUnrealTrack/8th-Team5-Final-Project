#include "Core/Tag/Skill/HB_SkillParamTag.h"

namespace SkillParamTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Cooldown, "SkillParam.Cooldown", "이 스킬의 쿨타임 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_ManaCost, "SkillParam.ManaCost", "이 스킬의 마나 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Duration, "SkillParam.Duration", "자기 버프의 지속시간 배율");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Reach, "SkillParam.Reach", "돌진, 도약의 거리 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_ShieldRatio, "SkillParam.ShieldRatio", "방패 강타 - 방어력 전환 배율");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Damage, "SkillParam.Damage", "활 스킬 - 피해 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Radius, "SkillParam.Radius", "활 스킬 - 범위 반경(cm 최종값)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_MarkBonus, "SkillParam.MarkBonus", "사냥꾼의 표식 - 받는 피해 증가(최종값)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_MitigateBreak, "SkillParam.MitigateBreak", "패링 - 대상이 받는 피해 증가율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_MitigateTime, "SkillParam.MitigateTime", "패링 - 피해 증가 지속시간");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Distance, "SkillParam.Distance", "마법 스킬 - 거리(cm 최종값)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SkillParam_Slow, "SkillParam.Slow", "마법 스킬 - 감속(최종값 0~1)");
}
