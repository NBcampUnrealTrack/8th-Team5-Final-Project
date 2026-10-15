#include "HB_CueTag.h"

namespace CueTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit, "GameplayCue.Hit", "타격 큐의 부모. 직접 달지 말 것")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Slash, "GameplayCue.Hit.Slash", "베기 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Blunt, "GameplayCue.Hit.Blunt", "둔타 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Pierce, "GameplayCue.Hit.Pierce", "관통 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Default, "GameplayCue.Hit.Default", "성질 미지정 폴백")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Block, "GameplayCue.Hit.Block", "방어로 막힘")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Parry, "GameplayCue.Hit.Parry", "패링 성공")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_World, "GameplayCue.Hit.World", "발사체가 벽·사물·시체 등 피해를 안 받는 곳에 맞음. 피 없음")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Heavy, "GameplayCue.Hit.Heavy", "워든 기본공격 · 도약 착지")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Claw, "GameplayCue.Hit.Claw", "딜버 발톱")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Bite, "GameplayCue.Hit.Bite", "딜버 물기")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Magic, "GameplayCue.Hit.Magic", "마법")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Spike, "GameplayCue.Hit.Spike", "가시 함정")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Skill_WarCry, "GameplayCue.Skill.WarCry", "전투의 함성 발동 연출")
}
