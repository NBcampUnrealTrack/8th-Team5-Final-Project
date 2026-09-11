#pragma once
#include "NativeGameplayTags.h"

namespace ItemTag
{
	// 아래와 같은 형식으로 선언
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Team_Player);
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Team_Enemy);

	// ---------------- 무엇을 들었나 ----------------
	// 기본공격 BP 의 AssetTags 가 이걸 쓴다. 무기 종류마다 BP 하나.
	// DT_Item 의 WeaponType 문자열로 "Weapon.<타입>" 을 만들어 찾는다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Sword)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Staff)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Bow)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Unarmed)

	// ---------------- 어떻게 막나 ----------------
	// 방어 BP 의 AssetTags 가 이걸 쓴다. 방어 수단마다 BP 하나.
	//
	// 왜 Weapon.* 를 재활용하지 않나: 기본공격 BP 가 이미 그 태그를 쓴다.
	// 같은 태그로 발동을 걸면 공격과 방어가 둘 다 뜨려고 한다.
	//
	// 해당 BP 가 없으면 TryActivateAbilitiesByTag 가 그냥 실패한다.
	// 그게 곧 "그 무기로는 방어할 수 없다" 다. 별도 검사가 필요 없다.
	//
	// 쳐낼 조건: 방어 모션이 무기와 무관해지면 Block.Shield 와 Block.Default 둘로 줄인다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Shield)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Sword)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Staff)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Bow)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Unarmed)
}
