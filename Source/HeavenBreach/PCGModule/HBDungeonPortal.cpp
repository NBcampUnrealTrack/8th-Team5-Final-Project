#include "PCGModule/HBDungeonPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Level.h"

AHBDungeonPortal::AHBDungeonPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PortalRoot"));
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetBoxExtent(FVector(100, 100, 150));
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetGenerateOverlapEvents(false);
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->ShapeColor = FColor::Cyan;
	Arrival = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrival"));
	Arrival->SetupAttachment(RootComponent);
	Arrival->SetRelativeLocation(FVector(250, 0, 100));
	Arrival->ArrowColor = FColor::Green;
}

bool AHBDungeonPortal::Contains(const FVector& Point) const
{
	if (!IsValid(Trigger) || Point.ContainsNaN()) return false;
	const FVector Local = Trigger->GetComponentTransform().InverseTransformPosition(Point).GetAbs();
	const FVector Extent = Trigger->GetUnscaledBoxExtent();
	return Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z;
}

bool AHBDungeonPortal::ValidateDefinition(FString& Error) const
{
	Error.Reset();
	auto Fail = [&](const TCHAR* Message) { Error = Message; return false; };
	if (PortalId.IsNone()) return Fail(TEXT("Assign a nonempty PortalId unique within this room map."));
	if (!IsValid(Trigger) || !IsValid(Arrival)) return Fail(TEXT("Portal requires its Trigger and Arrival components."));
	for (const FTransform& T : {GetActorTransform(), Arrival->GetComponentTransform()})
		if (!T.IsValid() || !T.GetScale3D().Equals(FVector::OneVector, 0.0001))
			return Fail(TEXT("Portal and arrival transforms must be finite with unit scale."));
	const FTransform TriggerTransform = Trigger->GetComponentTransform();
	if (!TriggerTransform.IsValid() || TriggerTransform.GetScale3D().GetMin() <= 0)
		return Fail(TEXT("Portal trigger transform must be finite with positive scale."));
	const FVector E = Trigger->GetUnscaledBoxExtent();
	const FVector Scaled = Trigger->GetScaledBoxExtent();
	if (E.ContainsNaN() || E.GetMin() <= 0 || Scaled.ContainsNaN() || Scaled.GetMin() <= 0)
		return Fail(TEXT("Portal trigger must have positive finite physical extents."));
	if (Contains(Arrival->GetComponentLocation())) return Fail(TEXT("Move Arrival outside this portal's trigger box."));
	if (!FMath::IsFinite(ReentryDelay) || ReentryDelay < 0.1f) return Fail(TEXT("Portal ReentryDelay must be at least 0.1 seconds."));
	return true;
}

bool AHBDungeonPortal::ConnectTo(AHBDungeonPortal* Other)
{
	FString Error;
	if (!GetWorld() || GetNetMode() == NM_Client || !HasAuthority() || !IsValid(Other) || !Other->HasAuthority()
		|| Other == this || Other->GetWorld() != GetWorld()
		|| !ValidateDefinition(Error) || !Other->ValidateDefinition(Error)) return false;
	Disconnect(); Other->Disconnect();
	TargetPortal = Other; Other->TargetPortal = this;
	return true;
}

AHBDungeonPortal* AHBDungeonPortal::FindInLevel(ULevel* Level, FName Id, FString& Error)
{
	Error.Reset();
	if (!IsValid(Level) || Id.IsNone()) { Error = TEXT("Missing portal level or PortalId."); return nullptr; }
	AHBDungeonPortal* Found = nullptr;
	for (AActor* Actor : Level->Actors)
	{
		if (auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal) && Portal->PortalId == Id)
		{
			if (Found) { Error = TEXT("Duplicate PortalId in the selected level instance."); return nullptr; }
			Found = Portal;
		}
	}
	if (!Found) { Error = TEXT("Selected PortalId is missing from the level instance."); return nullptr; }
	return Found->ValidateDefinition(Error) ? Found : nullptr;
}

void AHBDungeonPortal::Disconnect()
{
	if (IsValid(TargetPortal) && TargetPortal->TargetPortal == this)
	{
		TargetPortal->TargetPortal = nullptr;
		TargetPortal->BlockedPawns.Reset();
	}
	TargetPortal = nullptr;
	BlockedPawns.Reset();
}

bool AHBDungeonPortal::TryTeleport(APawn* Pawn)
{
	if (!bTravelEnabled || !GetWorld() || GetNetMode() == NM_Client || !HasAuthority() || !IsValid(Pawn) || !Pawn->HasAuthority()
		|| !Pawn->GetController() || !Pawn->GetController()->IsPlayerController() || Pawn->GetWorld() != GetWorld()
		|| !IsValid(TargetPortal) || !TargetPortal->HasAuthority() || TargetPortal->GetWorld() != GetWorld()
		|| !TargetPortal->bTravelEnabled || TargetPortal->TargetPortal != this || !Contains(Pawn->GetActorLocation())) return false;
	const TWeakObjectPtr<APawn> Key(Pawn);
	if (BlockedPawns.Contains(Key)) return false;
	const double Until = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, ReentryDelay);
	BlockedPawns.Add(Key, Until);
	const FTransform Destination = TargetPortal->Arrival->GetComponentTransform();
	// Respect the authored arrival. TeleportTo alone may search beyond a wall for an adjusted spot.
	if (GetWorld()->EncroachingBlockingGeometry(Pawn, Destination.GetLocation(), Destination.Rotator())) return false;
	if (!Pawn->TeleportTo(Destination.GetLocation(), Destination.Rotator())) return false;
	TargetPortal->BlockedPawns.Add(Key, GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, TargetPortal->ReentryDelay));
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
	if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		Controller->SetControlRotation(Destination.Rotator());
		Controller->ClientSetRotation(Destination.Rotator(), true);
	}
	Pawn->ForceNetUpdate();
	return true;
}

void AHBDungeonPortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld() || GetNetMode() == NM_Client || !HasAuthority() || !GetWorld()->IsGameWorld()) return;
	for (auto It = BlockedPawns.CreateIterator(); It; ++It)
		if (!It.Key().IsValid() || (GetWorld()->GetTimeSeconds() >= It.Value() && !Contains(It.Key()->GetActorLocation()))) It.RemoveCurrent();
	if (!IsValid(TargetPortal)) return;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (APlayerController* PC = It->Get()) TryTeleport(PC->GetPawn());
}

void AHBDungeonPortal::EndPlay(const EEndPlayReason::Type Reason)
{
	Disconnect();
	Super::EndPlay(Reason);
}
