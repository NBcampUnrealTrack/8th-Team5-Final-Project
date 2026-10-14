#include "Core/Tag/Melee/HB_MeleeTag.h"

namespace MeleeTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Counter, "State.Melee.Counter", "받아치기 상태 - 평타 중 패링");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Bloodthirst, "State.Melee.Bloodthirst", "피의 갈증 상태 - 처치 시 체력 회복");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_SteadyStance, "State.Melee.SteadyStance", "견고한 자세 상태 - 홀드 방어 스태미나 감소");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_Whirlwind, "Cooldown.Melee.Whirlwind", "회전 베기 재사용 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Melee_Whirlwind_Power, "Augment.Melee.Whirlwind.Power", "회전 베기 증강 - 위력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Melee_Whirlwind_Wide, "Augment.Melee.Whirlwind.Wide", "회전 베기 증강 - 확장");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_Bloodthirst_Boost, "Augment.Passive.Bloodthirst.Boost", "피의 갈증 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_SteadyStance_Boost, "Augment.Passive.SteadyStance.Boost", "견고한 자세 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_IronHeart_Boost, "Augment.Passive.IronHeart.Boost", "강철 심장 패시브 증강 - 최대 체력 추가");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_CombatInstinct_Boost, "Augment.Passive.CombatInstinct.Boost", "전투 본능 패시브 증강 - 최대 공격력 보너스 추가");
}
