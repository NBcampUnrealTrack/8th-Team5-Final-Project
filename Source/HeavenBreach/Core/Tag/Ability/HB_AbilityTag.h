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

	// 패링 어빌리티 식별. 패링 BP 가 AssetTag 로 갖는다.
	// TryParry 가 이 태그로 발동시키므로, 이 태그가 없는 BP 는 R 에 반응하지 않는다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Parry);

	// 이 비용 요청이 일반공격에서 왔다는 표식.
	//
	// 왜 필요한가: 전투 호흡이 일반공격 마나만 줄인다. 스킬·달리기·방어는 안 줄인다.
	// 그 판별을 예전에는 "넘어온 태그가 지금 든 무기 태그와 같은가" 로 했는데,
	// 무기를 바꾸면 판단이 흔들리고 읽는 사람이 의도를 알 수 없었다.
	//
	// 작동방식: 일반공격 경로가 ModifyManaCost 에 이 태그를 넘기고, 거기서 이것만 본다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_BasicAttack);

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