// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "HeavenBreachGameState.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMatchStateChanged,
	FName,
	NewMatchState
);

UCLASS()
class HEAVENBREACH_API AHeavenBreachGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchStateChanged OnMatchStateChanged;

	// 서버에서 시간 설정 
	void SetMatchEndServerTime(double InEndServerTime);

	// client/ server 모두 같은 기준으로 시간 계산
	UFUNCTION(BlueprintPure, Category = "Match|Timer")
	double GetRemainingMatchTime();

	UFUNCTION(BlueprintPure, Category = "Match|Timer")
	double GetMatchEndServerTime() const
	{
		return MatchEndServerTime;
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


protected:
	// MatchState 변경 전달
	virtual void OnRep_MatchState() override;

	UPROPERTY(
		ReplicatedUsing = OnRep_MatchEndServerTime,
		BlueprintReadOnly, Category = "Match|Timer"
		)
	double MatchEndServerTime = 0.0f;

	UFUNCTION()
	void OnRep_MatchEndServerTime();
};
