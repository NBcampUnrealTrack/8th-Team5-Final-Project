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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Dagger)
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

	// ---------------- 숫자 옵션이 고르는 스탯 ----------------
	// FItemOption::Stat 의 드롭다운에 뜨는 목록.
	// 새 스탯은 여기와 Item/ItemStatBinding.cpp 의 표 두 곳에 더한다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_AttackPower)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_Defense)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_MaxHP)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_MaxMana)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_AttackSpeed)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_MoveSpeed)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_ManaRegen)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_Cooldown)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_SkillDamage)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_HeadshotDamage)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_DamageTaken)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_MeleeDamageTaken)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_HealingReceived)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stat_HealthRegen)

	// ---------------- 장비 합계를 GE 로 넘기는 이름표 ----------------
	// UGE_EquipmentStats 가 값을 이 이름표로 받는다 (SetByCaller). DT 에서 쓸 일은 없다.
	// + 를 받는 스탯은 _Flat, % 를 받는 스탯은 _Percent 가 있다.
	//
	// 왜 태그인가: 문자열이었다면 BonusStatType 처럼 오타가 조용히 0 이 되는 사고가 여기서도 났다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_AttackPower_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_AttackPower_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_Defense_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_Defense_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MaxHP_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MaxHP_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MaxMana_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MaxMana_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_AttackSpeed_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MoveSpeed_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MoveSpeed_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_ManaRegen_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_ManaRegen_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_Cooldown_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_Cooldown_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_SkillDamage_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_HeadshotDamage_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_DamageTaken_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_MeleeDamageTaken_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_HealingReceived_Percent)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_HealthRegen_Flat)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Equipment_HealthRegen_Percent)

	// 아이템 동작 효과의 자기 피해 GE(UGE_ItemSelfDamage)가 양을 받는 이름표 (SetByCaller).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemEffect_Magnitude)

	// ---------------- 동작 효과가 켜져 있다는 표시 ----------------
	// DT_ItemEffect 정의의 Tag 드롭다운에 뜨는 목록. 장착하면 UGE_ItemEffectTag 가 끼운 사람에게 붙인다.
	// 코드는 이 태그들을 하나하나 모른다 — 동작은 정의의 원형(Archetype)이 정한다.
	// 새 효과의 태그는 여기 말고 에디터 태그 관리자에서 만들어도 된다. C++ 을 안 고쳐도 된다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Effect_GuardAura)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Effect_SkillHPCost)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Effect_OverflowBurn)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Effect_LowHealthExposed)
}
