#include "Core/Tag/Archer/HB_ArcherTag.h"
namespace ArcherTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_EagleEye, "Passive.Archer.EagleEye");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_LongRangeShot, "Passive.Archer.LongRangeShot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_WeakPointShot, "Passive.Archer.WeakPointShot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_LightFootsteps, "Passive.Archer.LightFootsteps");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_HunterSense, "Passive.Archer.HunterSense");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_TrapExpert, "Passive.Archer.TrapExpert");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_TacticalRetreat, "Passive.Archer.TacticalRetreat");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_QuickReload, "Passive.Archer.QuickReload");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_PoisonArrow, "Passive.Archer.PoisonArrow");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Tracked, "State.Archer.Tracked");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Smoked, "State.Archer.Smoked");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_EvasiveShot, "Cooldown.Archer.EvasiveShot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_BasicAttack, "Cooldown.Archer.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_SwiftMovement, "Cooldown.Archer.SwiftMovement");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_PiercingArrow, "Cooldown.Archer.PiercingArrow");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_BindingTrap, "Cooldown.Archer.BindingTrap");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_MultiShot, "Cooldown.Archer.MultiShot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_TrackingMark, "Cooldown.Archer.TrackingMark");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_ExplosiveArrow, "Cooldown.Archer.ExplosiveArrow");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_RapidFire, "Cooldown.Archer.RapidFire");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_SmokeArrow, "Cooldown.Archer.SmokeArrow");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_PiercingShot_Power, "Augment.Bow.PiercingShot.Power", "관통 사격 강화 - 위력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_PiercingShot_Rapid, "Augment.Bow.PiercingShot.Rapid", "관통 사격 강화 - 신속");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_HunterMark_Deep, "Augment.Bow.HunterMark.Deep", "사냥꾼의 표식 강화 - 깊은 표식");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_HunterMark_Wide, "Augment.Bow.HunterMark.Wide", "사냥꾼의 표식 강화 - 넓은 표식");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_EvasiveShot_Rapid, "Augment.Bow.EvasiveShot.Rapid", "회피 사격 강화 - 신속");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Bow_EvasiveShot_Light, "Augment.Bow.EvasiveShot.Light", "회피 사격 강화 - 가벼움");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_SwiftStep_Boost, "Augment.Passive.SwiftStep.Boost", "재빠른 발걸음 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_WeakPoint_Boost, "Augment.Passive.WeakPoint.Boost", "약점 사격 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_HunterBreath_Boost, "Augment.Passive.HunterBreath.Boost", "사냥꾼의 호흡 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Tracked_Deep1, "State.Archer.Tracked.Deep1", "표식 - 깊은 표식 1단계(+25%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Tracked_Deep2, "State.Archer.Tracked.Deep2", "표식 - 깊은 표식 2단계(+30%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Tracked_Deep3, "State.Archer.Tracked.Deep3", "표식 - 깊은 표식 3단계(+35%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Bow_HunterMark, "GameplayCue.Bow.HunterMark", "사냥꾼의 표식 - 대상 머리 위 표식(표식 GE 가 붙어 있는 동안)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Bow_HunterMarkCast, "GameplayCue.Bow.HunterMarkCast", "사냥꾼의 표식 - 적용 순간 손 입자·범위 펄스");
}
