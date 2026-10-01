#include "Player/HBFootstepComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HBPlayerCharacter.h"
#include "Sound/SoundBase.h"

UHBFootstepComponent::UHBFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHBFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소리를 들을 사람이 없는 전용 서버에서는 셀 필요가 없다.
	if (GetNetMode() == NM_DedicatedServer || !bUseDistanceSteps)
	{
		SetComponentTickEnabled(false);
	}
}

void UHBFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const float Speed = Movement->Velocity.Size2D();
	const bool bMoving = Movement->IsMovingOnGround() && Speed >= MinStepSpeed;
	if (!bMoving)
	{
		bWasMoving = false;
		return;
	}

	const bool bSprinting = Character->IsSprinting();
	const float StepDistance = bSprinting ? SprintStepDistance : WalkStepDistance;

	// 막 걷기 시작했으면 반 보폭에서 시작해 첫 발이 너무 늦지 않게 한다.
	if (!bWasMoving)
	{
		DistanceSinceStep = StepDistance * 0.5f;
		bWasMoving = true;
	}

	DistanceSinceStep += Speed * DeltaTime;
	if (DistanceSinceStep >= StepDistance)
	{
		DistanceSinceStep = FMath::Fmod(DistanceSinceStep, StepDistance);
		PlayFootstep(bSprinting);
	}
}

void UHBFootstepComponent::PlayFootstep(bool bSprinting)
{
	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetOwner());
	if (!Character || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USoundBase* Sound = bSprinting ? SprintSound.Get() : WalkSound.Get();
	if (!Sound)
	{
		return;
	}

	float Volume = bSprinting ? SprintVolume : WalkVolume;
	if (!bSprinting && Character->bIsCrouched)
	{
		Volume *= CrouchVolumeScale;
	}

	// 발밑에서 낸다. 남에게도 발소리 위치로 들린다.
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector FootLocation = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	UGameplayStatics::PlaySoundAtLocation(this, Sound, FootLocation, Volume);
}
