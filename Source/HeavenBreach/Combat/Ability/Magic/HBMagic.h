#pragma once
#include "CoreMinimal.h"

/** 마법 스킬 순수 계산. 상태를 갖지 않아 테스트하기 쉽다. */
namespace HBMagic
{
	/** 시전 시간 배율. 집중 없음 1.0 / 집중 0.75 / 강화 1·2·3단계 0.65·0.55·0.45. */
	inline float CastTimeMultiplier(bool bHasFocus, int32 FocusBoostTier)
	{
		if (!bHasFocus) return 1.f;
		switch (FMath::Min(FocusBoostTier, 3))
		{
		case 1: return 0.65f;
		case 2: return 0.55f;
		case 3: return 0.45f;
		default: return 0.75f; // Tier <= 0 은 집중 기본
		}
	}

	/**
	 * 마력 순환 액티브 스태미나 배율(설계 2.1). 패시브 없음 1.0 / 마력 순환 0.80 / 강화 1·2·3단계 0.70·0.60·0.50.
	 * 액티브 스킬 비용에만 곱한다(달리기·방어 제외). 전역 Player_ManaCostMultiplier 와는 별개다.
	 */
	inline float ManaCycleMultiplier(bool bHasManaCycle, int32 BoostTier)
	{
		if (!bHasManaCycle) return 1.f;
		switch (FMath::Min(BoostTier, 3))
		{
		case 1: return 0.70f;
		case 2: return 0.60f;
		case 3: return 0.50f;
		default: return 0.80f; // Tier <= 0 은 마력 순환 기본
		}
	}

	/** 잔류 마력 기록에 남기는 최근 시전 ID 수. 이보다 오래된 시전의 투사체가 늦게 맞으면 다시 트리거될 수 있다. */
	constexpr int32 MaxResidualConsumed = 16;

	/**
	 * 한 시전당 잔류 마력 쿨감은 한 번만. 이미 쓴 ID 목록에 없으면 추가하고 true.
	 * 마지막 ID 하나만 기억하면 시전이 겹칠 때(화살 비행 중 다른 시전) 앞 시전이 다시 트리거된다.
	 * 목록은 최근 MaxResidualConsumed 개만 남긴다(오래된 것부터 버림).
	 */
	inline bool ConsumeResidualTrigger(int32 CastId, TArray<int32>& InOutConsumed)
	{
		if (CastId <= 0 || InOutConsumed.Contains(CastId)) return false;
		InOutConsumed.Add(CastId);
		if (InOutConsumed.Num() > MaxResidualConsumed)
		{
			InOutConsumed.RemoveAt(0, InOutConsumed.Num() - MaxResidualConsumed);
		}
		return true;
	}

	/** 잔류 마력 쿨감 배율. 0단계 1.0 / 1·2·3단계 1.5·2.0·2.5. */
	inline float ResidualReduction(int32 BoostTier)
	{
		return 1.f + 0.5f * FMath::Clamp(BoostTier, 0, 3);
	}

	/** 시전 몽타주 재생 속도 = 애셋 재생 속도 / 캐스팅 배율. 집중이면 모션도 그만큼 빨라진다. */
	inline float CastMontagePlayRate(float MontagePlayRate, float CastMult)
	{
		return FMath::Max(MontagePlayRate, KINDA_SMALL_NUMBER) / FMath::Max(CastMult, KINDA_SMALL_NUMBER);
	}

	/** 발동까지 걸리는 실제 시간(s). ReleaseTime 은 몽타주(재생 속도 1) 기준 시각이라 위 재생 속도로 나눈다. */
	inline float CastReleaseDelay(float ReleaseTime, float MontagePlayRate, float CastMult)
	{
		return FMath::Max(ReleaseTime, 0.f) / CastMontagePlayRate(MontagePlayRate, CastMult);
	}

	/**
	 * 순간 이동 목적지 검사(지면 trace 이후). 지면 법선 Z 가 MinNormalZ 이상, 높이차(오르막·내리막 모두)가 MaxHeightDelta 이하, 이동 거리가 MinMove 초과여야 통과.
	 */
	inline bool IsTeleportDestinationValid(float GroundNormalZ, float HeightDelta, float MoveDistance, float MinNormalZ, float MaxHeightDelta, float MinMove)
	{
		return GroundNormalZ >= MinNormalZ && FMath::Abs(HeightDelta) <= MaxHeightDelta && MoveDistance > MinMove;
	}

	/**
	 * 순간 이동 도착 캡슐을 지면 위로 띄우는 높이.
	 * 캡슐 아래 반구는 경사(법선 Z = cos θ)에 닿을 때 중심이 접점보다 Radius / cos θ 위에 있어야 한다.
	 * 그런데 지면 trace 는 캡슐 중심 바로 아래 점을 재므로, 그 점 + Radius(평지 기준) 로 놓으면 Radius × (1/cos θ − 1) 만큼 경사에 파묻힌다.
	 * 그만큼 더 올리고, 여유 Extra(평지에서도 바닥에 딱 붙어 겹침 판정이 나지 않게)를 더한다. 법선 Z 가 0 에 가까우면 터지지 않게 막는다.
	 */
	inline float TeleportSlopeLift(float Radius, float GroundNormalZ, float Extra)
	{
		return FMath::Max(Radius, 0.f) * (1.f / FMath::Max(GroundNormalZ, 0.1f) - 1.f) + Extra;
	}
}
