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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SetByCaller_Heal);

	// 쿨다운 GE 의 지속시간. 어빌리티가 증강 배율을 반영한 값을 넘긴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SetByCaller_Cooldown);

	// 자기 버프 GE 의 지속시간. 베이스가 SelfBuffEffect 에 넘긴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SetByCaller_Duration);

	// 코스트 GE 의 마나 소비량.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SetByCaller_ManaCost);
}

// 기본공격 스킬 식별자 상수
namespace HBSkillId
{
	const FName BasicAttack(TEXT("BasicAttack"));
}