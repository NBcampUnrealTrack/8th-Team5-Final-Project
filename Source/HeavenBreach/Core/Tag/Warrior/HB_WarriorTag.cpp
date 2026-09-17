#include "Core/Tag/Warrior/HB_WarriorTag.h"

namespace WarriorTag
{
	// ---------------- 패시브 보유 ----------------
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_CombatBreath, "Passive.Warrior.CombatBreath");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_StrongBody, "Passive.Warrior.StrongBody");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_BerserkerRage, "Passive.Warrior.BerserkerRage");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_WeaponMaster, "Passive.Warrior.WeaponMaster");

	// ---------------- 상태 ----------------
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_BloodFrenzy, "State.Warrior.BloodFrenzy");

	// ---------------- 쿨타임 ----------------
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_BattleCry, "Cooldown.Warrior.BattleCry");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_ChargeAttack, "Cooldown.Warrior.ChargeAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Smash, "Cooldown.Warrior.Smash");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_BloodFrenzy, "Cooldown.Warrior.BloodFrenzy");

	// ---------------- 증강: 액티브 ----------------
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BattleCry_Efficiency, "Augment.Warrior.BattleCry.Efficiency");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BattleCry_Duration, "Augment.Warrior.BattleCry.Duration");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BattleCry_Damage, "Augment.Warrior.BattleCry.Damage");

	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_ChargeAttack_Efficiency, "Augment.Warrior.ChargeAttack.Efficiency");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_ChargeAttack_Guard, "Augment.Warrior.ChargeAttack.Guard");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_ChargeAttack_Reach, "Augment.Warrior.ChargeAttack.Reach");

	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_Smash_Efficiency, "Augment.Warrior.Smash.Efficiency");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_Smash_Damage, "Augment.Warrior.Smash.Damage");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_Smash_Shield, "Augment.Warrior.Smash.Shield");

	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BloodFrenzy_Efficiency, "Augment.Warrior.BloodFrenzy.Efficiency");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BloodFrenzy_Lifesteal, "Augment.Warrior.BloodFrenzy.Lifesteal");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_BloodFrenzy_Bleed, "Augment.Warrior.BloodFrenzy.Bleed");

	// ---------------- 증강: 패시브 ----------------
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_CombatBreath_Free, "Augment.Warrior.CombatBreath.Free");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_StrongBody_Defense, "Augment.Warrior.StrongBody.Defense");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_Rage_AttackSpeed, "Augment.Warrior.Rage.AttackSpeed");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Augment_WeaponMaster_Damage, "Augment.Warrior.WeaponMaster.Damage");

	// ================ 폐기 예정 ================
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_CombatInstinct, "Passive.Warrior.CombatInstinct");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_ConsecutiveAttack, "Passive.Warrior.ConsecutiveAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_KillingMomentum, "Passive.Warrior.KillingMomentum");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_WeaknessDetection, "Passive.Warrior.WeaknessDetection");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_CombatRecovery, "Passive.Warrior.CombatRecovery");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Passive_Revenge, "Passive.Warrior.Revenge");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_BloodThirst, "State.Warrior.BloodThirst");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_BloodThirst, "Cooldown.Warrior.BloodThirst");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_ChargeSlash, "Cooldown.Warrior.ChargeSlash");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Execute, "Cooldown.Warrior.Execute");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_CombatRoll, "Cooldown.Warrior.CombatRoll");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_ChainPull, "Cooldown.Warrior.ChainPull");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_BattleCry_AttackMultiplier, "Data.Warrior.BattleCry.AttackMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_BattleCry_MoveSpeedBonus, "Data.Warrior.BattleCry.MoveSpeedBonus");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_BattleCry_DefenseBonus, "Data.Warrior.BattleCry.DefenseBonus");
}
