#include "HB_AbilityTag.h"

namespace AbilityTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,"State.Dead")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Offensive_Melee, "Ability.Offensive.Melee", "근접 공격 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Death, "Ability.Death", "사망 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SetByCaller_Damage, "SetByCaller.Damage", "GE 에 데미지 수치를 넘기는 통로");
}