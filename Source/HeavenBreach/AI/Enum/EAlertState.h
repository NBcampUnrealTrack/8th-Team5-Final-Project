// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEnemyAI, Log, All);

/**
 * 플레이어를 아는가만 담당한다.
 * 그 안에서 어찌 싸울지는 서브트리에서 진행
 */
UENUM(BlueprintType)
enum class EAlertState : uint8
{
	Idle    UMETA(DisplayName = "Idle"),      // 플레이어 모름
	Alerted UMETA(DisplayName = "Alerted"),   // 소리나 누군가의 경고신호를 받음, 수상함
	Engaged UMETA(DisplayName = "Engaged"),   // 플레이어 인지 중
	Lost    UMETA(DisplayName = "Lost"),      // 놓침, 수색 중
};


/**
 * 디버깅 용도
 */
inline const TCHAR* LexToString(EAlertState State)
{
	switch (State)
	{
	case EAlertState::Idle:    return TEXT("Idle");
	case EAlertState::Alerted: return TEXT("Alerted");
	case EAlertState::Engaged: return TEXT("Engaged");
	case EAlertState::Lost:    return TEXT("Lost");
	default:                   return TEXT("Unknown");
	}
}