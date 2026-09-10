// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HBPostMatchTypes.generated.h"

/** Why the player ended up on the post-match results map (ExtractionResult). */
UENUM(BlueprintType)
enum class EHBPostMatchReason : uint8
{
	None,
	Extracted,
	Died,
	// The match ended around the player (time expired, no active players left, etc.)
	// rather than a personal death or extraction.
	MatchEnded
};
