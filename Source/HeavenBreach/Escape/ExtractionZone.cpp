// Copyright HeavenBreach Team5. All Rights Reserved.


#include "ExtractionZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

AExtractionZone::AExtractionZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(FName("BlockAll"));

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(Mesh);
	Zone->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionObjectType(ECC_WorldStatic);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Zone->OnComponentEndOverlap.AddDynamic(this, &AExtractionZone::OnZoneEndOverlap);
	}
}

void AExtractionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<APawn*, FTimerHandle>& Pair : ChannelingPlayers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	ChannelingPlayers.Empty();

	Super::EndPlay(EndPlayReason);
}

bool AExtractionZone::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (Pawn == nullptr)
	{
		return false;
	}

	return IsPawnInZone(Pawn) && !ChannelingPlayers.Contains(const_cast<APawn*>(Pawn));
}

void AExtractionZone::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	if (Pawn == nullptr || !IsPawnInZone(Pawn) || ChannelingPlayers.Contains(Pawn))
	{
		return;
	}

	const AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}

	StartChannel(Pawn);
}

FText AExtractionZone::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

void AExtractionZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (ChannelingPlayers.Contains(Pawn))
		{
			CancelChannel(Pawn);
		}
	}
}

bool AExtractionZone::IsPawnInZone(const APawn* Pawn) const
{
	return Pawn != nullptr && Zone != nullptr && Zone->IsOverlappingActor(Pawn);
}

void AExtractionZone::StartChannel(APawn* Pawn)
{
	FTimerHandle& Handle = ChannelingPlayers.Add(Pawn);

	if (ExtractionDuration <= 0.0f)
	{
		CompleteChannel(Pawn);
		return;
	}

	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AExtractionZone::CompleteChannel, Pawn);
	GetWorld()->GetTimerManager().SetTimer(Handle, Del, ExtractionDuration, false);

	BP_OnExtractStarted(Pawn, ExtractionDuration);
}

void AExtractionZone::CancelChannel(APawn* Pawn)
{
	if (FTimerHandle* Handle = ChannelingPlayers.Find(Pawn))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		ChannelingPlayers.Remove(Pawn);
		BP_OnExtractCanceled(Pawn);
	}
}

void AExtractionZone::CompleteChannel(APawn* Pawn)
{
	if (!ChannelingPlayers.Contains(Pawn))
	{
		return;
	}
	ChannelingPlayers.Remove(Pawn);

	if (!IsValid(Pawn))
	{
		return;
	}

	ExtractPawn(Pawn);
	BP_OnExtractCompleted(Pawn);
}

void AExtractionZone::ExtractPawn(APawn* Pawn)
{
	if (AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr)
	{
		GameMode->NotifyPlayerExtracted(Pawn->GetPlayerState());
	}

	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);
}
