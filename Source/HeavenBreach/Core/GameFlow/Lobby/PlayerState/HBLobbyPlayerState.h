// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "HBLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnReadyStateChanged,
	bool,
	bNewReadyState
	);
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyPlayerState : public AHB_PlayerState
{
	GENERATED_BODY()
	
public:
	AHBLobbyPlayerState();
	
	bool IsReady() const { return bIsReady; }
	
	void SetReady(bool bNewReady);
	
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnReadyStateChanged OnReadyStateChanged;
	
protected:
	UPROPERTY(
		ReplicatedUsing = OnRep_IsReady,
		BlueprintReadOnly,
		Category = "Lobby|Ready")
	bool bIsReady = false;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_IsReady();
};
