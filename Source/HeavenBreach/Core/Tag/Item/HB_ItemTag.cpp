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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Equipment_Defense, "Data.Equipment.Defense", "장착 방어구 방어력 합계")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Equipment_AttackPower, "Data.Equipment.AttackPower", "장착 무기 공격력 합계")
}