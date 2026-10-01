// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameState.h"

#include "Net/UnrealNetwork.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonDoorSeal.h"

bool AHeavenBreachGameState::PublishDungeonLayout(int32 GenerationId, int32 Seed, const TArray<FHBPrototypeRoom>& Rooms,
        TSubclassOf<AHBDungeonDoorSeal> SealClass, const FVector& SealExtent)
{
    if (!HasAuthority() || GetNetMode() == NM_Client || GenerationId <= DungeonSnapshot.GenerationId) return false;
    FString Error;
    if (!HBDungeonRuntimeValidation::ValidateLayoutForLoading(Rooms, Error)
        || !AHBDungeonDoorSeal::ValidateSettings(SealClass, SealExtent, Error)) return false;
    DungeonSnapshot.GenerationId = GenerationId;
    DungeonSnapshot.Seed = Seed;
    DungeonSnapshot.Rooms = Rooms;
    DungeonSnapshot.DoorSealClass = SealClass.Get();
    DungeonSnapshot.DoorSealExtent = SealExtent;
    ForceNetUpdate();
    OnDungeonSnapshotChanged.Broadcast();
    return true;
}

void AHeavenBreachGameState::OnRep_DungeonSnapshot()
{
    OnDungeonSnapshotChanged.Broadcast();
}


void AHeavenBreachGameState::SetMatchEndServerTime(double InEndServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchEndServerTime = InEndServerTime;
}

double AHeavenBreachGameState::GetRemainingMatchTime()
{
	if (MatchEndServerTime <= 0.0)
	{
		return 0.0;
	}

	return FMath::Max(0.0f, MatchEndServerTime - GetServerWorldTimeSeconds());
}

void AHeavenBreachGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeavenBreachGameState, MatchEndServerTime);
    DOREPLIFETIME(AHeavenBreachGameState, DungeonSnapshot);
}

void AHeavenBreachGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	const FName CurrentMatchState = GetMatchState();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM|MatchState : %s"),
		*CurrentMatchState.ToString()
	);

	OnMatchStateChanged.Broadcast(CurrentMatchState);
}

void AHeavenBreachGameState::OnRep_MatchEndServerTime()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("End: %.2f | Now: %.2f | Remaining: %.2f"),
		MatchEndServerTime,
		GetServerWorldTimeSeconds(),
		GetRemainingMatchTime()
	);
}
