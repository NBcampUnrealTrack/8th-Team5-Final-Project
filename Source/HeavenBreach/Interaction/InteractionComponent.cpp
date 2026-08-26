// Copyright HeavenBreach Team5. All Rights Reserved.


#include "InteractionComponent.h"
#include "InteractableInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UpdateFocus();
}

void UInteractionComponent::UpdateFocus()
{
	FVector ViewLocation;
	FVector ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		SetFocusedInteractable(nullptr);
		return;
	}

	const FVector TraceEnd = ViewLocation + ViewDirection * InteractionDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), false);
	Params.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, TraceChannel, Params);

	AActor* Candidate = nullptr;
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UInteractableInterface>()
			&& IInteractableInterface::Execute_CanInteract(HitActor, GetOwner()))
		{
			Candidate = HitActor;
		}
	}

	SetFocusedInteractable(Candidate);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FColor LineColor = (Candidate != nullptr) ? FColor::Green : FColor::Red;
		DrawDebugLine(GetWorld(), ViewLocation, TraceEnd, LineColor, false, -1.0f, 0, 1.0f);
	}
#endif
}

void UInteractionComponent::SetFocusedInteractable(AActor* NewInteractable)
{
	if (FocusedInteractable == NewInteractable)
	{
		return;
	}

	FocusedInteractable = NewInteractable;
	OnFocusChanged.Broadcast(NewInteractable);
}

void UInteractionComponent::TryInteract()
{
	if (FocusedInteractable == nullptr)
	{
		return;
	}

	Server_Interact(FocusedInteractable);
}

bool UInteractionComponent::Server_Interact_Validate(AActor* Target)
{
	return true;
}

void UInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
	APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || Target == nullptr)
	{
		return;
	}

	const float MaxDistance = InteractionDistance + ServerDistanceTolerance;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return;
	}

	if (Target->Implements<UInteractableInterface>()
		&& IInteractableInterface::Execute_CanInteract(Target, Pawn))
	{
		IInteractableInterface::Execute_Interact(Target, Pawn);
	}
}

FText UInteractionComponent::GetCurrentPrompt() const
{
	if (FocusedInteractable && FocusedInteractable->Implements<UInteractableInterface>())
	{
		return IInteractableInterface::Execute_GetInteractPrompt(FocusedInteractable);
	}
	return FText::GetEmpty();
}

bool UInteractionComponent::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr)
	{
		return false;
	}

	if (const AController* Controller = Pawn->GetController())
	{
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		OutLocation = Location;
		OutDirection = Rotation.Vector();
		return true;
	}

	OutLocation = Pawn->GetActorLocation();
	OutDirection = Pawn->GetActorForwardVector();
	return true;
}

APawn* UInteractionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

void UInteractionComponent::Client_OpenContainer_Implementation(UInventoryComponent* Container)
{
	OnRequestOpenContainer.Broadcast(Container);
}
