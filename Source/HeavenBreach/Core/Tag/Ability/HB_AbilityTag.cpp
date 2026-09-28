#include "HB_AbilityTag.h"

namespace AbilityTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,"State.Dead")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Offensive_Melee, "Ability.Offensive.Melee", "근접 공격 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Offensive_Ranged, "Ability.Offensive.Ranged", "원거리 공격 어빌리티 식별자");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Attacking, "State.Attacking", "공격 중");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Block,"Cooldown.Block")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Death, "Ability.Death", "사망 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_HitReact, "Ability.HitReact", " 피격 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Parry, "Ability.Parry", "패링 어빌리티 식별. 패링 BP 가 AssetTag 로 갖는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_BasicAttack, "Ability.BasicAttack", "비용 요청의 출처가 일반공격");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SetByCaller_Damage, "SetByCaller.Damage", "GE 에 데미지 수치를 넘기는 통로");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SetByCaller_Heal, "SetByCaller.Heal", "GE 에 회복 수치를 넘기는 통로");

	UE_DEFINE_GAMEPLAY_TAG(TAG_SetByCaller_Cooldown, "SetByCaller.Cooldown");
	UE_DEFINE_GAMEPLAY_TAG(TAG_SetByCaller_Duration, "SetByCaller.Duration");
	UE_DEFINE_GAMEPLAY_TAG(TAG_SetByCaller_ManaCost, "SetByCaller.ManaCost");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_SetByCaller_AugmentMagnitude, "SetByCaller.AugmentMagnitude", "증강 GE 에 단계 수치를 넘기는 통로");
}