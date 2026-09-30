#include "HB_ItemTag.h"

namespace ItemTag
{
	// 아래와 같은 형식으로 정의
	// UE_DEFINE_GAMEPLAY_TAG(TAG_Team_Player,"Team.Player")
	// UE_DEFINE_GAMEPLAY_TAG(TAG_Team_Enemy,"Team.Enemy")

	UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Sword, "Weapon.Sword")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Staff, "Weapon.Staff")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Bow, "Weapon.Bow")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Unarmed, "Weapon.Unarmed")

	UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Shield, "Block.Shield")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Sword, "Block.Sword")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Staff, "Block.Staff")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Bow, "Block.Bow")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Unarmed, "Block.Unarmed")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_AttackPower, "Stat.AttackPower", "공격력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_Defense, "Stat.Defense", "방어력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_MaxHP, "Stat.MaxHP", "최대 체력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_MaxMana, "Stat.MaxMana", "최대 마나")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_AttackSpeed, "Stat.AttackSpeed", "공격 속도. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_MoveSpeed, "Stat.MoveSpeed", "이동 속도. + 는 실제 속도")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_ManaRegen, "Stat.ManaRegen", "마나 회복. + 는 초당 회복량")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_Cooldown, "Stat.Cooldown", "쿨다운. 줄이려면 -. + 는 초")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_SkillDamage, "Stat.SkillDamage", "스킬 피해. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_HeadshotDamage, "Stat.HeadshotDamage", "헤드샷 피해. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_DamageTaken, "Stat.DamageTaken", "받는 피해. 덜 맞으려면 -. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_MeleeDamageTaken, "Stat.MeleeDamageTaken", "근접으로 받는 피해. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_HealingReceived, "Stat.HealingReceived", "회복 효과. % 만")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Stat_HealthRegen, "Stat.HealthRegen", "초당 체력 회복. + 는 초당 회복량, % 는 그 배율")

	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_AttackPower_Flat, "Data.Equipment.AttackPower.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_AttackPower_Percent, "Data.Equipment.AttackPower.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_Defense_Flat, "Data.Equipment.Defense.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_Defense_Percent, "Data.Equipment.Defense.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MaxHP_Flat, "Data.Equipment.MaxHP.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MaxHP_Percent, "Data.Equipment.MaxHP.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MaxMana_Flat, "Data.Equipment.MaxMana.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MaxMana_Percent, "Data.Equipment.MaxMana.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_AttackSpeed_Percent, "Data.Equipment.AttackSpeed.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MoveSpeed_Flat, "Data.Equipment.MoveSpeed.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MoveSpeed_Percent, "Data.Equipment.MoveSpeed.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_ManaRegen_Flat, "Data.Equipment.ManaRegen.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_ManaRegen_Percent, "Data.Equipment.ManaRegen.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_Cooldown_Flat, "Data.Equipment.Cooldown.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_Cooldown_Percent, "Data.Equipment.Cooldown.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_SkillDamage_Percent, "Data.Equipment.SkillDamage.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_HeadshotDamage_Percent, "Data.Equipment.HeadshotDamage.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_DamageTaken_Percent, "Data.Equipment.DamageTaken.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_MeleeDamageTaken_Percent, "Data.Equipment.MeleeDamageTaken.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_HealingReceived_Percent, "Data.Equipment.HealingReceived.Percent")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_HealthRegen_Flat, "Data.Equipment.HealthRegen.Flat")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Equipment_HealthRegen_Percent, "Data.Equipment.HealthRegen.Percent")

	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ItemEffect_Magnitude, "Data.ItemEffect.Magnitude")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Item_Effect_GuardAura, "Item.Effect.GuardAura", "자신·파티원 방어력 증가")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Item_Effect_SkillHPCost, "Item.Effect.SkillHPCost", "스킬 사용 시 체력 소모")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Item_Effect_OverflowBurn, "Item.Effect.OverflowBurn", "마나가 가득 차면 주기적 자기 피해")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Item_Effect_LowHealthExposed, "Item.Effect.LowHealthExposed", "체력 30% 이하 시 방어력 대폭 감소")
}