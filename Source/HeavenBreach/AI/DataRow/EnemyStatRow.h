// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemyStatRow.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FEnemyStatRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// ----- 감각 -----
	UPROPERTY(EditAnywhere, Category = "Sense", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	// SightRadius 이하로 두면 경계선에서 발각/해제가 깜빡인다.
	// LoseSightRadius가 SightRadius보다 낮아도 aicontrollor측에 방어코드 존재함
	UPROPERTY(EditAnywhere, Category = "Sense", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 2000.f;

	// 정면 기준 반각. 60 이면 총 120도
	UPROPERTY(EditAnywhere, Category = "Sense", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionAngle = 60.f;

	// 0일경우 영원히 기억
	UPROPERTY(EditAnywhere, Category = "Sense", meta = (ClampMin = "0.1"))
	float SightMaxAge = 2.f;

	// 목격된 대상이 시야각을 벗어나도 볼수 잇는 거리
	// -1이면 비활성
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AutoSuccessSightRange = -1.f;

	// ----- 기억 -----
	// Lost 유지 시간. 지나면 Idle
	UPROPERTY(EditAnywhere, Category = "Alert", meta = (ClampMin = "0.0"))
	float LostMemoryDuration = 4.f;

	// ----- 이동 -----
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.f;

	// MoveTo 도착 판정 반경. 비행 적이 잇을 경우 300 이상 추천
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 100.f;

	// 스폰지점으로 부터 배회 반경
	UPROPERTY(EditAnywhere, Category = "Behavior", meta = (ClampMin = "0.0"))
	float MaxMoveDistance = 1000.f;

	// ----- 전투 -----
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float Defense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float AttackRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float TurnSpeed = 360.f;
};
