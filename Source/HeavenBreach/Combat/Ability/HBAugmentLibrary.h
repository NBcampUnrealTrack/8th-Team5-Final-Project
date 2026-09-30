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
	 * 한 통에서 DrawCount 장을 무작위로 뽑는다. 모자라면 있는 만큼만.
	 *
	 * 기획서 28.1: 액티브 강화·패시브 강화·신규 패시브·공통 강화를 한 통에 섞어 뽑는다.
	 * 전에는 공통 강화를 따로 먼저 1장 뽑아서 매 화면에 공통이 꼭 1장 있었다.
	 * 쳐낼 조건: 증강 등급·가중치가 기획에 생기면 여기서 가중 추첨으로 바꾼다.
	 *
	 * Pool 은 뽑은 항목이 제거되므로 참조로 받는다. 한 번 뽑힌 것은 다시 안 나온다(28.3 중복 금지).
	 */
	HEAVENBREACH_API void DrawCandidates(
		TArray<FHBUpgradeCandidate>& Pool,
		int32 DrawCount,
		FRandomStream& Rng,
		TArray<FHBUpgradeCandidate>& Out);
}
