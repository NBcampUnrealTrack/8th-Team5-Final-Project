#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBSkillAugment.h"

/**
 * 증강의 단계 계산과 후보 뽑기. 액터도 월드도 쓰지 않는 순수 함수다.
 *
 * 왜 PlayerState 밖으로 뺐나: AHB_PlayerState 는 액터라서 자동화 테스트에서
 * 만들기 어렵다. 그리고 상한과 채우기가 섞인 뽑기 규칙은 경계 조건이 많아
 * 눈으로 검증하기 어렵다. 규칙만 떼어내면 테스트가 붙는다.
 */
namespace HBAugment
{
	// 증강이 올라갈 수 있는 최대 단계. TierMagnitudes 가 비면 1 이다.
	HEAVENBREACH_API int32 GetMaxTier(const FHBSkillAugment& Augment);

	// 보유 목록에서 그 증강의 현재 단계. 없으면 0.
	HEAVENBREACH_API int32 FindTier(const TArray<FHBAugmentStack>& Stacks, FGameplayTag AugmentTag);

	/**
	 * 단계를 1 올린다. 목록에 없으면 1단계로 추가한다.
	 * 이미 MaxTier 면 아무것도 바꾸지 않고 false 를 준다.
	 */
	HEAVENBREACH_API bool RaiseTier(TArray<FHBAugmentStack>& Stacks, FGameplayTag AugmentTag, int32 MaxTier);

	// 그 단계의 수치. 단계가 범위를 벗어나면 0.
	HEAVENBREACH_API float GetTierMagnitude(const FHBSkillAugment& Augment, int32 Tier);

	/**
	 * 후보 3장을 뽑는다.
	 *
	 * 규칙
	 *   1. CommonPool 에서 최대 CommonCap 장
	 *   2. 나머지를 Pool 에서
	 *   3. Pool 이 모자라면 CommonPool 에서 더 채운다
	 *
	 * 3번이 상한보다 우선한다. 후반에는 스킬 증강을 다 먹고 패시브도 다 가져서
	 * Pool 이 비는데, 상한을 고집하면 카드가 1장만 뜬다.
	 *
	 * Pool 과 CommonPool 은 뽑은 항목이 제거되므로 참조로 받는다.
	 */
	HEAVENBREACH_API void DrawCandidates(
		TArray<FHBUpgradeCandidate>& Pool,
		TArray<FHBUpgradeCandidate>& CommonPool,
		int32 DrawCount,
		int32 CommonCap,
		FRandomStream& Rng,
		TArray<FHBUpgradeCandidate>& Out);
}
