#pragma once
#include "NativeGameplayTags.h"

namespace StateTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Blocking)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_InventoryBusy)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_BoxInteracting)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Staggered)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_HitReact)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CoolTime_Attack_Melee)

	// 적 공격 쿨타임의 부모. CoolTime.Attack.Beam · Leap 같은 스킬별 쿨타임이 전부 이 아래에 있다.
	// StateTree 의 Recover 조건과 추격 시간 계산이 "어느 공격이든 쿨타임 중인가" 를 이걸로 본다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_CoolTime_Attack)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Aiming)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_SuperArmor)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_AfterDelay)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Grabbed)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryWindow)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryRecovery)

	// 액티브 스킬 발동 중. UHBPlayerActiveGABase 가 발동하는 동안만 붙인다.
	// 기본 공격과 스킬이 같은 근접 트레이스를 쓰기 때문에, 이 태그로 스킬 타격을 가른다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_UsingSkill)

	// 활 스킬 조준 중(관통 사격 당기기, 표식 조준). 활 기본 조준과 가르기 위한 태그.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Bow_SkillAiming)

	// ----- 공격 성질. 공격하는 쪽 ASC 에 붙는다 — 공격 GA 의 Activation Owned Tags 에 넣는다 -----
	// 맞는 쪽(AHBPlayerCharacter::ApplyMeleeHit)이 맞는 순간 공격자 태그를 보고 판단한다.
	// 타격 소리(GameplayCue.Hit.*)를 공격자 태그로 고르는 것과 같은 방식이다.
	// State.Attacking 의 자식이라 "공격 중" 검사에도 잡힌다 — 실제로 공격 중이라 뜻이 맞다.
	// Asset Tags 에 넣으면 ASC 에 안 붙어서 조용히 안 걸린다. 반드시 Activation Owned Tags 다.

	// 그냥 맞으면 경직. 막거나 패링하면 없다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking_Stagger)

	// 막기 · 패링이 통하지 않는다. 방어태세여도 그냥 맞은 것으로 친다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking_Unblockable)

	// 시전 중(마법 스킬 시전 시간 동안). 시전 중 이동 제한·중단 판정에 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Casting)
}
