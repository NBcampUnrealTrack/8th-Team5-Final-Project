#include "HB_StateTag.h"

namespace StateTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Attacking,"State.Attacking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Blocking,"State.Blocking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_InventoryBusy,"State.InventoryBusy")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_BoxInteracting,"State.BoxInteracting")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attack_Melee, "CoolTime.Attack.Melee", "근접 공격 후딜");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoolTime_Attack, "CoolTime.Attack", "적 공격 쿨타임의 부모. 스킬별 쿨타임이 전부 이 아래");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Staggered, "State.Staggered", "경직 - 행동 중단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_HitReact, "State.HitReact", "피격 리액션 - 행동 유지");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Aiming, "State.Aiming", "조준 중. 이 구간에만 타겟을 따라 회전한다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_SuperArmor, "State.SuperArmor", "경직으로 끊기지 않는 구간");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_AfterDelay, "State.AfterDelay", "공격후 가만히 있을 후딜레이");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Grabbed, "State.Grabbed", "그랩 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_ParryWindow, "State.ParryWindow", "패링 판정 창. 이 동안 정면 근접 피격은 무효 + 상대 스태거");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_ParryRecovery, "State.ParryRecovery", "패링 실패 후딜. 이 동안 재패링 불가. 공격은 막지 않는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_UsingSkill, "State.UsingSkill", "액티브 스킬 발동 중. 스킬 피해 옵션이 이걸 보고 걸린다");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Bow_SkillAiming, "State.Bow.SkillAiming", "활 스킬 조준 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attacking_Stagger, "State.Attacking.Stagger", "공격 중 - 그냥 맞으면 경직 (막거나 패링하면 없음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attacking_Unblockable, "State.Attacking.Unblockable", "공격 중 - 막기 · 패링 불가");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Casting, "State.Casting", "시전 중");
}
