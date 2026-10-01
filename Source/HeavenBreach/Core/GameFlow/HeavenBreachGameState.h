// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "PCGModule/HBDungeonSnapshot.h"
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
    bool PublishDungeonLayout(int32 GenerationId, int32 Seed, const TArray<FHBPrototypeRoom>& Rooms,
        TSubclassOf<AHBDungeonDoorSeal> SealClass, const FVector& SealExtent);
    bool InvalidateDungeonLayout(int32 GenerationId, const FString& FailureReason = FString());
    const FHBDungeonSnapshot& GetDungeonSnapshot() const { return DungeonSnapshot; }
    FSimpleMulticastDelegate OnDungeonSnapshotChanged;

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
    UPROPERTY(ReplicatedUsing=OnRep_DungeonSnapshot)
    FHBDungeonSnapshot DungeonSnapshot;
    UFUNCTION()
    void OnRep_DungeonSnapshot();

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
