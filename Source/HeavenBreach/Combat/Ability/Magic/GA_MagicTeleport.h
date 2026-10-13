// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/Magic/HBPlayerCastGABase.h"
#include "GA_MagicTeleport.generated.h"

/**
 * 순간 이동 (마법 액티브, 설계 2.1). 0.3초 뒤 바라보는 방향(Yaw)으로 지면 위 700cm 를 순간 이동한다.
 * 벽·낭떠러지·높이차(1m 초과)로 갈 수 없으면 비용 없이 끝난다(CanRelease 에서 목적지를 검사한다).
 * 성공하면 출발·도착 지점에서 각각 큐를 한 번씩 실행한다. 증강: 장거리(거리), 재빠른(쿨다운).
 */
UCLASS()
class HEAVENBREACH_API UGA_MagicTeleport : public UHBPlayerCastGABase
{
	GENERATED_BODY()

public:
	UGA_MagicTeleport();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	float GetDistance() const { return Distance; }
	float GetWallMargin() const { return WallMargin; }
	float GetMinGroundNormalZ() const { return MinGroundNormalZ; }
	float GetMaxHeightDelta() const { return MaxHeightDelta; }
	float GetMinMove() const { return MinMove; }
	float GetGroundLift() const { return GroundLift; }

protected:
	virtual bool CanRelease(const FGameplayAbilityActorInfo* Info) override;
	virtual void OnCastReleased(const FGameplayAbilityActorInfo* Info) override;

	/** 기본 이동 거리. 장거리 증강(SkillParam.Distance)이 이 값을 대신한다 */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", Units = "cm"))
	float Distance = 700.f;

	/** 벽에 막혔을 때 막힌 지점에서 물러나는 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", Units = "cm"))
	float WallMargin = 12.f;

	/** 도착 지면 법선 Z 최소값. 이보다 가파르면 이동하지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinGroundNormalZ = 0.7f;

	/** 도착 지면과 현재 발밑의 최대 높이차(오르막·내리막). 수평 sweep 도 이만큼 올린 높이에서 한다(경사·계단에 걸리지 않게) */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxHeightDelta = 100.f;

	/** 이보다 조금만 움직이면 이동하지 않는다(벽에 붙어 쓴 경우) */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", Units = "cm"))
	float MinMove = 40.f;

	/** 도착 캡슐을 지면 위로 더 띄우는 여유. 경사 보정(반지름 × (1/법선Z − 1))에 더해진다(HBMagic::TeleportSlopeLift) */
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundLift = 2.f;

private:
	/** CanRelease 가 검증한 목적지(캡슐 중심). OnCastReleased 가 쓴다 */
	FVector PendingDestination = FVector::ZeroVector;
};
