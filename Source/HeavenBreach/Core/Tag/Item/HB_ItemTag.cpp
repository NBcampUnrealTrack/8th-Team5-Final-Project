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
}