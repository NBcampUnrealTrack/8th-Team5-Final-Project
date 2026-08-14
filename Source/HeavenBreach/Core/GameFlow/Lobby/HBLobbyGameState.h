// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "HBLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCanStartMatchChanged,
	bool,
	bCanStartMatch
	);
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	bool CanStartMatch() const {return bCanStartMatch;}
	
	void SetCanStartMatch(bool bNewCanStartMatch);
	
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnCanStartMatchChanged OnCanStartMatchChanged;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(
		ReplicatedUsing = OnRep_CanStartMatch,
		BlueprintReadOnly,
		Category = "Lobby")
	bool bCanStartMatch = false;
	
	UFUNCTION()
	void OnRep_CanStartMatch();
};
