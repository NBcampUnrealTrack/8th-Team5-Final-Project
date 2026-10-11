#pragma once
#include "NativeGameplayTags.h"

// 근접 스킬 태그. 쿨다운·증강·상태.
namespace MeleeTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Counter);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Bloodthirst);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_SteadyStance);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Whirlwind);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Melee_Whirlwind_Power);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Melee_Whirlwind_Wide);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Passive_Bloodthirst_Boost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Passive_SteadyStance_Boost);
}
