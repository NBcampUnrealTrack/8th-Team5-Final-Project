// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/GameFlow/HBPostMatchTypes.h"
#include "HBMatchResultSubsystem.generated.h"

/**
 * Carries why the local player arrived at the post-match results map (died vs.
 * extracted vs. match ended). The travel to that map is a plain ClientTravel,
 * so PlayerState/GameState do not survive it - the GameInstance (and this
 * subsystem) does, since it lives for the whole client process.
 *
 * Set server-side via AHBPlayerController::ClientSetPostMatchReason right
 * before ClientTravel. AHBPlayerController::TryShowPostMatchResultWidget()
 * reads it back on the ExtractionResult map to decide which result widget
 * (extraction success vs. death) to create.
 */
UCLASS()
class HEAVENBREACH_API UHBMatchResultSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Match|Result")
	void SetPostMatchReason(EHBPostMatchReason NewReason) { PostMatchReason = NewReason; }

	UFUNCTION(BlueprintPure, Category = "Match|Result")
	EHBPostMatchReason GetPostMatchReason() const { return PostMatchReason; }

private:
	UPROPERTY()
	EHBPostMatchReason PostMatchReason = EHBPostMatchReason::None;
};
