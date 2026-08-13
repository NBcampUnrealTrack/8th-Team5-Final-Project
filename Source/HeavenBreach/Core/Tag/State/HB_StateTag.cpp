#include "HB_StateTag.h"

namespace StateTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Attacking,"State.Attacking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Blocking,"State.Blocking")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_InventoryBusy,"State.InventoryBusy")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_BoxInteracting,"State.BoxInteracting")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attack_Melee, "CoolTime.Attack.Melee", "근접 공격 후딜");
}