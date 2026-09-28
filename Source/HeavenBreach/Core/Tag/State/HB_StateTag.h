#pragma once
#include "NativeGameplayTags.h"

namespace StateTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Blocking)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_InventoryBusy)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_BoxInteracting)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Staggered)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_HitReact)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CoolTime_Attack_Melee)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Aiming)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_SuperArmor)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_AfterDelay)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Grabbed)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryWindow)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryRecovery)

	// 액티브 스킬 발동 중. UHBPlayerActiveGABase 가 발동하는 동안만 붙인다.
	// 기본 공격과 스킬이 같은 근접 트레이스를 쓰기 때문에, 이 태그로 스킬 타격을 가른다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_UsingSkill)
}
