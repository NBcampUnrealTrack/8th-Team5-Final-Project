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

protected:
	// MatchState 변경 전달
	virtual void OnRep_MatchState() override;
};
