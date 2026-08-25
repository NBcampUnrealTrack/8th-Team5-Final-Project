#pragma once
#include "NativeGameplayTags.h"

namespace AbilityTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Offensive_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Offensive_Ranged);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Block);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_HitReact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SetByCaller_Damage);
}
