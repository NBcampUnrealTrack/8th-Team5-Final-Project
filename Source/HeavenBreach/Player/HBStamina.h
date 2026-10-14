// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 스태미나·마나 소모·회복 계산. 캐릭터의 자원 타이머(AHBPlayerCharacter::UpdateResources)가 쓴다.
 * 스태미나 = 달리기·방어 유지, 마나 = 액티브 스킬(Docs/스태미나-마나-분리-설계.md).
 */
namespace HBStamina
{
	/** 달리기 초당 스태미나. 배율 0 이면 공짜(전투 호흡) */
	inline float SprintCostPerSecond(float BaseCostPerSecond, float SprintMultiplier)
	{
		return FMath::Max(0.f, BaseCostPerSecond) * FMath::Max(0.f, SprintMultiplier);
	}

	/** 방어 유지 초당 스태미나. 견고한 자세 배율을 곱한다 */
	inline float BlockCostPerSecond(float BaseCostPerSecond, float SteadyStanceMultiplier)
	{
		return FMath::Max(0.f, BaseCostPerSecond * SteadyStanceMultiplier);
	}

	/** 이번 틱에 실제로 깎을 양. 남은 것보다 많이 깎지 않는다 */
	inline float SpendAmount(float Current, float CostPerSecond, float Interval)
	{
		return FMath::Min(FMath::Max(0.f, Current), FMath::Max(0.f, CostPerSecond) * Interval);
	}

	/** 스태미나 회복량. 마지막 소모 뒤 RegenDelay 가 지나야 찬다 */
	inline float StaminaRegenAmount(float Current, float Max, float RegenPerSecond, float RegenMultiplier, float Interval, double SecondsSinceUse, float RegenDelay)
	{
		if (SecondsSinceUse < RegenDelay)
		{
			return 0.f;
		}
		const float Amount = FMath::Max(0.f, RegenPerSecond * RegenMultiplier) * Interval;
		return FMath::Max(0.f, FMath::Min(Max - Current, Amount));
	}

	/** 마나 회복량. 대기 없이 항상 찬다. (기본 + 장비 Flat) × 배율 */
	inline float ManaRegenAmount(float Current, float Max, float RegenPerSecond, float RegenFlat, float RegenMultiplier, float Interval)
	{
		const float Amount = FMath::Max(0.f, (RegenPerSecond + RegenFlat) * RegenMultiplier) * Interval;
		return FMath::Max(0.f, FMath::Min(Max - Current, Amount));
	}

	/** 자원 타이머 한 틱의 입력. 달리기·방어 비용은 그 행동 중일 때만 0 보다 크다(배율까지 곱한 초당 값) */
	struct FResourceTickInput
	{
		float Mana = 0.f;
		float MaxMana = 0.f;
		float ManaRegenPerSecond = 0.f;
		float ManaRegenFlat = 0.f;
		float ManaRegenMultiplier = 1.f;

		float Stamina = 0.f;
		float MaxStamina = 0.f;
		float StaminaRegenPerSecond = 0.f;
		float StaminaRegenMultiplier = 1.f;
		float StaminaRegenDelay = 0.f;
		double SecondsSinceStaminaUse = 0.0;

		float SprintCostPerSecond = 0.f;
		float BlockCostPerSecond = 0.f;

		float Interval = 0.25f;
	};

	/** 한 틱의 결과. 캐릭터가 Delta 를 속성에 더하고, 사용이면 사용 시각을 기록하고, 멈춤이면 달리기·방어를 푼다 */
	struct FResourceTickResult
	{
		float ManaDelta = 0.f;
		float StaminaDelta = 0.f;
		bool bStaminaUsed = false;
		bool bStopStaminaActions = false;
	};

	/**
	 * 한 틱 계산. 마나는 대기 없이 항상 찬다(달리기·방어·스태미나 바닥과 무관).
	 * 스태미나는 실제로 깎이는 게 있으면 쓰고(바닥나면 멈춤), 없으면 대기 뒤 찬다. 공짜 달리기(전투 호흡)는 사용이 아니다.
	 */
	inline FResourceTickResult ComputeTick(const FResourceTickInput& In)
	{
		FResourceTickResult Out;
		Out.ManaDelta = ManaRegenAmount(In.Mana, In.MaxMana, In.ManaRegenPerSecond, In.ManaRegenFlat, In.ManaRegenMultiplier, In.Interval);

		const float CostPerSecond = FMath::Max(0.f, In.SprintCostPerSecond) + FMath::Max(0.f, In.BlockCostPerSecond);
		if (CostPerSecond > 0.f)
		{
			const float Spend = SpendAmount(In.Stamina, CostPerSecond, In.Interval);
			Out.StaminaDelta = -Spend;
			Out.bStaminaUsed = true;
			Out.bStopStaminaActions = In.Stamina <= Spend + KINDA_SMALL_NUMBER;
			return Out;
		}

		Out.StaminaDelta = StaminaRegenAmount(In.Stamina, In.MaxStamina, In.StaminaRegenPerSecond, In.StaminaRegenMultiplier,
			In.Interval, In.SecondsSinceStaminaUse, In.StaminaRegenDelay);
		return Out;
	}
}
