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

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.1"))
	float AttackSpeedRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 200.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float TurnSpeed = 360.f;

	// ----- 경험치 -----
	UPROPERTY(EditAnywhere, Category = "Exp", meta = (ClampMin = "0.0"))
	float ExpReward = 40.f;

	// ----- 드랍 -----
	// 이 적이 쓸 DT_EnemyLootTable 의 행.
	//
	// 왜 핸들인가: 테이블과 행 이름은 항상 같이 다닌다. 핸들로 두면 에디터가
	// 행 이름을 드롭다운으로 주므로 오타가 불가능해진다. FName 이면 오타가
	// 그대로 저장되고 적을 죽여봐야 발견된다.
	//
	// 쳐낼 조건: 적마다 테이블이 달라질 일이 없어지면 테이블을 한곳으로 빼도 된다.
	UPROPERTY(EditAnywhere, Category = "Loot")
	FDataTableRowHandle LootRow;

	// ----- 전술 -----
	// 평소에 배회하지 않고 제자리에서 대기한다
	UPROPERTY(EditAnywhere, Category = "Tactics")
	bool bStayAtHome = false;

	// 쿨다운 동안 포위 · 후퇴한다. false 면 제자리 대기.
	// 이 적에게 전술 이동 기능 전체를 켜는 스위치다. 기존 적은 false 로 둔다.
	UPROPERTY(EditAnywhere, Category = "Tactics")
	bool bUseSpacing = false;

	// 타겟과 유지하려는 간격. 이보다 가까우면 확률과 무관하게 물러난다.
	//
	// 왜 필요한가: 선회 반지름을 "지금 거리" 로만 쓰면, 붙어 있을 때 선회 지점이
	// AcceptanceRadius 안에 들어와 MoveTo 가 즉시 완료되고 제자리에 선다.
	// 그리고 전술을 확률로만 뽑으면 붙은 채로 계속 도는 구간이 생긴다.
	//
	// 쳐낼 조건: 붙어서 싸우는 게 맞는 적이면 0 으로 두면 예전 동작이 된다.
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0"))
	float KeepDistance = 350.f;

	// 선회 한 번에 도는 각도. 작을수록 원에 가깝지만 그만큼 자주 갱신해야 한다
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float OrbitStepDegrees = 35.f;

	// 선회 대신 뒤로 물러날 확률
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RetreatChance = 0.3f;

	// 뒤로 물러날 거리
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0"))
	float BackstepDistance = 400.f;

	// 전술을 새로 고를 때 타겟도 바꿀 확률. 0 이면 절대 안 바꾼다
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RetargetChance = 0.f;

	// 사거리 밖에서 쫓기만 하는 시간이 이 범위(초)에서 뽑은 값을 넘으면 타겟을 다시 고른다.
	// 기본으로 모든 적에게 켜져 있다 (팀 결정). 특정 적만 끄려면 그 줄의 최대를 0 으로 둔다.
	//
	// 왜 필요한가: 닿지 않는 플레이어(높은 곳, 막힌 길)만 계속 쫓으면
	// 옆에 있는 다른 플레이어를 무시하는 것처럼 보인다.
	// 바꿀 후보는 지금 보이는 플레이어뿐이다. 한 명만 보이면 그대로 계속 쫓는다.
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0"))
	float ChaseRetargetTimeMin = 3.f;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ClampMin = "0.0"))
	float ChaseRetargetTimeMax = 5.f;
};
