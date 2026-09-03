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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnStartConfirmationChanged,
	bool,
	bNewStartConfirmed
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
	bool IsStartConfirmed() const { return bStartConfirmed; }
	
	void SetReady(bool bNewReady);
	void SetStartConfirmed(bool bNewStartConfirmed);
	
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnReadyStateChanged OnReadyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Start")
	FOnStartConfirmationChanged OnStartConfirmationChanged;
	
protected:
	UPROPERTY(
		ReplicatedUsing = OnRep_IsReady,
		BlueprintReadOnly,
		Category = "Lobby|Ready")
	bool bIsReady = false;

	/** Kept separate from ready so every player must explicitly approve the final match start. */
	UPROPERTY(
		ReplicatedUsing = OnRep_StartConfirmed,
		BlueprintReadOnly,
		Category = "Lobby|Start")
	bool bStartConfirmed = false;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_StartConfirmed();
};
