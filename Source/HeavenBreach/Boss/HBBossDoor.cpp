#include "Boss/HBBossDoor.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

void AHBBossDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHBBossDoor, bLocked);
}

bool AHBBossDoor::CanInteract_Implementation(AActor* Interactor) const
{
	return !bLocked && Super::CanInteract_Implementation(Interactor);
}

void AHBBossDoor::Interact_Implementation(AActor* Interactor)
{
	if (bLocked)
	{
		return;
	}
	Super::Interact_Implementation(Interactor);
}

FText AHBBossDoor::GetInteractPrompt_Implementation() const
{
	return bLocked ? LockedPrompt : Super::GetInteractPrompt_Implementation();
}

void AHBBossDoor::SetDoorState(bool bOpen, bool bLock, bool bInstant)
{
	if (!HasAuthority())
	{
		return;
	}
	bLocked = bLock;
	if (bIsOpen == bOpen)
	{
		return;
	}
	if (!bInstant)
	{
		if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		{
			StateChangeServerTime = GameState->GetServerWorldTimeSeconds();
		}
	}
	bIsOpen = bOpen;
	OnRep_IsOpen();
}
