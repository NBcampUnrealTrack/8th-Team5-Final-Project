#include "HB_StateTag.h"

namespace StateTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Attacking,"State.Attacking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Blocking,"State.Blocking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_InventoryBusy,"State.InventoryBusy")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_BoxInteracting,"State.BoxInteracting")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attack_Melee, "CoolTime.Attack.Melee", "근접 공격 후딜");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Staggered, "State.Staggered", "경직 - 행동 중단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_HitReact, "State.HitReact", "피격 리액션 - 행동 유지");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Aiming, "State.Aiming", "조준 중. 이 구간에만 타겟을 따라 회전한다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_SuperArmor, "State.SuperArmor", "경직으로 끊기지 않는 구간");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_AfterDelay, "State.AfterDelay", "공격후 가만히 있을 후딜레이");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Grabbed, "State.Grabbed", "그랩 상태");
}