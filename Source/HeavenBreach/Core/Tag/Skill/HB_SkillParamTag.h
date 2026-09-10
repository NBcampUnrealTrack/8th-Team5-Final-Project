#pragma once
#include "NativeGameplayTags.h"

/**
 * 스킬 수치의 "항목" 이름표.
 *
 * 왜 넣었나: 증강 하나가 여러 수치를 바꿀 수 있어야 하는데(쿨 50% + 마나 30%),
 * 값 하나짜리 칸으로는 표현이 안 된다. 항목 태그를 키로 쓰는 맵으로 바꾸면서
 * 그 키의 어휘를 여기 모았다.
 *
 * 작동방식: 증강 애셋의 Params 에 "이 태그 → 이 숫자" 로 적고,
 * 스킬 코드가 GetAugmentParam(태그, 기본값) 으로 꺼낸다.
 *
 * Augment.* 아래 두지 않는다. 항목은 증강이 아니다.
 * 증강 태그(Augment.Warrior.Smash.Shield)는 "어떤 증강인가" 고,
 * 여기 태그(SkillParam.ShieldRatio)는 "이 숫자가 무엇인가" 다. 축이 다르다.
 *
 * 숫자의 뜻은 여기서 정하지 않는다. 쓰는 쪽이 정한다.
 * 배율로 쓸 거면 기본값 1.0, 더할 거면 기본값 0.0 을 넘긴다.
 *
 * 쳐낼 조건: 항목이 늘면 여기에 줄만 추가한다. 직업별로 나누지 않는다.
 * 나누면 "이 항목이 어디 있더라" 를 두 곳에서 찾게 된다.
 */
namespace SkillParamTag
{
	// ---- 모든 액티브가 공통으로 갖는 것 ----

	// 이 스킬의 쿨타임 배율. 0.5 면 절반. 전역 쿨감(어트리뷰트)보다 먼저 곱해진다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_Cooldown);

	// 이 스킬의 마나 배율. 0 으로 두면 마나를 안 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_ManaCost);

	// 자기 버프의 지속시간 배율.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_Duration);

	// ---- 스킬 고유 ----

	// 돌진, 도약의 거리 배율.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_Reach);

	// 방패 강타 - 방어력의 몇 배를 공격력에 얹는가.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_ShieldRatio);

	// 패링 - 성공 시 대상이 받는 피해 증가율과 그 지속시간.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_MitigateBreak);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SkillParam_MitigateTime);
}
