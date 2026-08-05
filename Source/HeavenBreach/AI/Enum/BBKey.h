// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 키 값을 혼동하지 않기 위한 구조체
 */
struct FBBKey
{
	inline static const FName AlertState = FName(TEXT("AlertState"));
	inline static const FName TargetActor = FName(TEXT("TargetActor"));
	inline static const FName LastKnownLocation = FName(TEXT("LastKnownLocation"));
	inline static const FName HomeLocation = FName(TEXT("HomeLocation"));
	inline static const FName MoveGoal = FName(TEXT("MoveGoal"));
	inline static const FName MaxMoveDistance = FName(TEXT("MaxMoveDistance"));
};
