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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSelectedJobChanged,
	FName,
	NewSelectedJobId
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
	FName GetSelectedJobId() const { return SelectedJobId; }
	bool HasSelectedJob() const { return !SelectedJobId.IsNone(); }

	void SetReady(bool bNewReady);
	void SetStartConfirmed(bool bNewStartConfirmed);

	/** Server-authoritative. The JobId is still re-validated against UHBJobData when the match spawns the character. */
	void SetSelectedJobId(FName NewJobId);

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnReadyStateChanged OnReadyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Start")
	FOnStartConfirmationChanged OnStartConfirmationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Job")
	FOnSelectedJobChanged OnSelectedJobChanged;

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

	/** Matches UHBJobData::JobId (e.g. "Warrior", "Mage", "Archer"). Defaults to Warrior until Mage/Archer are ready to play. */
	UPROPERTY(
		ReplicatedUsing = OnRep_SelectedJobId,
		BlueprintReadOnly,
		Category = "Lobby|Job")
	FName SelectedJobId = NAME_None;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_StartConfirmed();

	UFUNCTION()
	void OnRep_SelectedJobId();
};
