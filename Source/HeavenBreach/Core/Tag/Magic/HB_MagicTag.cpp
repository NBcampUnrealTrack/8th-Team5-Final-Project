#include "Core/Tag/Magic/HB_MagicTag.h"

namespace MagicTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_ElementalOrb, "Cooldown.Magic.ElementalOrb", "원소 구체 재사용 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_Teleport, "Cooldown.Magic.Teleport", "순간 이동 재사용 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_FrostField, "Cooldown.Magic.FrostField", "서리 지대 재사용 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_ManaExplosion, "Cooldown.Magic.ManaExplosion", "마력 폭발 재사용 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_ElementalOrb_Power, "Augment.Magic.ElementalOrb.Power", "원소 구체 증강 - 위력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_ElementalOrb_Rapid, "Augment.Magic.ElementalOrb.Rapid", "원소 구체 증강 - 속사");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_ElementalOrb_Wide, "Augment.Magic.ElementalOrb.Wide", "원소 구체 증강 - 폭발 범위");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_Teleport_Far, "Augment.Magic.Teleport.Far", "순간 이동 증강 - 장거리");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_Teleport_Rapid, "Augment.Magic.Teleport.Rapid", "순간 이동 증강 - 재빠른");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_FrostField_Bitter, "Augment.Magic.FrostField.Bitter", "서리 지대 증강 - 혹한");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_FrostField_Wide, "Augment.Magic.FrostField.Wide", "서리 지대 증강 - 확장");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_ManaExplosion_Power, "Augment.Magic.ManaExplosion.Power", "마력 폭발 증강 - 위력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Magic_ManaExplosion_Wide, "Augment.Magic.ManaExplosion.Wide", "마력 폭발 증강 - 확장");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_Focus_Boost, "Augment.Passive.Focus.Boost", "집중 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_ManaCycle_Boost, "Augment.Passive.ManaCycle.Boost", "마력 순환 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_ResidualMana_Boost, "Augment.Passive.ResidualMana.Boost", "잔류 마력 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Augment_Passive_GlassCannon_Boost, "Augment.Passive.GlassCannon.Boost", "유리 대포 패시브 증강 - 강화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Focus, "State.Magic.Focus", "집중 상태 - 시전 시간 단축");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_ResidualMana, "State.Magic.ResidualMana", "잔류 마력 상태 - 쿨다운 감소 대기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Frosted, "State.Magic.Frosted", "서리 지대 감속 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_ManaCycle, "State.Magic.ManaCycle", "마력 순환 상태 - 액티브 마나 감소");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Magic_CastHand, "GameplayCue.Magic.CastHand", "마법 시전 손 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Magic_OrbImpact, "GameplayCue.Magic.OrbImpact", "원소 구체 적중 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Magic_Teleport, "GameplayCue.Magic.Teleport", "순간 이동 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Magic_ManaExplosion, "GameplayCue.Magic.ManaExplosion", "마력 폭발 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Magic_FrostField, "GameplayCue.Magic.FrostField", "서리 지대 생성 소리. 지대 이펙트는 AHBFrostField 가 직접 띄운다");
}
