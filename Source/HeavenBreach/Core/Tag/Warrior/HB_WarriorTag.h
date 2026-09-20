#pragma once

#include "NativeGameplayTags.h"

namespace WarriorTag
{
	// ---------------- 패시브 보유 ----------------
	// 패시브 GA 가 ActivationOwnedTags 로 붙인다. 보유 여부가 곧 이 태그다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_CombatBreath);      // 전투 호흡
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_StrongBody);        // 강인한 육체
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_BerserkerRage);     // 전사의 분노
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_WeaponMaster);      // 무기의 달인
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_GuardStance);      // 방어태세. 증강으로 획득

	// ---------------- 부여된 상태 ----------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_BloodFrenzy);         // 피의 광란 지속 중. 흡혈 판정이 본다

	// ---------------- 쿨타임 ----------------
	// 각각 쿨다운 GE 애셋 하나가 이 태그를 Granted Tag 로 갖는다.
	// CheckCooldown 이 애셋에 박힌 태그를 읽으므로 런타임 전달은 불가.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_BattleCry);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_ChargeAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Smash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_BloodFrenzy);

	// ---------------- 증강: 액티브 ----------------
	// Efficiency = 쿨타임 + 마나 동시 감소. 밸런스상 항상 같이 붙는다.
	// 부모 태그(Augment.Warrior.BattleCry)로 "이 스킬에 증강이 붙었나"를 물을 수 있다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BattleCry_Efficiency);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BattleCry_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BattleCry_Damage);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_ChargeAttack_Efficiency);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_ChargeAttack_Guard);      // 사용 중 피해감소
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_ChargeAttack_Reach);      // 도약 거리·속도

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Smash_Efficiency);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Smash_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Smash_Shield);            // 방패 강타로 변경

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BloodFrenzy_Efficiency);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BloodFrenzy_Lifesteal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_BloodFrenzy_Bleed);

	// ---------------- 증강: 패시브 ----------------
	// 지금은 패시브당 하나.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_CombatBreath_Free);       // 전투 호흡의 일반 공격 마나 0
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_StrongBody_Defense);      // 강인한 육체의 방어력도 증가
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_Rage_AttackSpeed);        // 분노의 공격의 속도도 상승
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Augment_WeaponMaster_Damage);     // 무기의 달인 공격력 상승

	// ================ 폐기 예정 ================
	// 지금 지우면 참조하는 코드가 컴파일되지 않는다. 참조된 코드가 있을수 있다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_CombatInstinct);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_ConsecutiveAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_KillingMomentum);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_WeaknessDetection);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_CombatRecovery);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Passive_Revenge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_BloodThirst);               // → TAG_State_BloodFrenzy
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_BloodThirst);            // → TAG_Cooldown_BloodFrenzy
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_ChargeSlash);            // → TAG_Cooldown_ChargeAttack
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Execute);                // → TAG_Cooldown_Smash
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_CombatRoll);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_ChainPull);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_BattleCry_AttackMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_BattleCry_MoveSpeedBonus);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_BattleCry_DefenseBonus);
}
