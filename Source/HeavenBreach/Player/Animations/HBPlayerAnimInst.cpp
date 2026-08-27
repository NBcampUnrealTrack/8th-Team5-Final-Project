#include "HBPlayerAnimInst.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/HBPlayerCharacter.h"

void UHBPlayerAnimInst::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwnerInst = Cast<AHBPlayerCharacter>(TryGetPawnOwner());
}

void UHBPlayerAnimInst::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerInst)
	{
		OwnerInst = Cast<AHBPlayerCharacter>(TryGetPawnOwner());
	}
	
	if (OwnerInst)
	{
		#pragma region AimOffset
		const FRotator AimDelta = UKismetMathLibrary::NormalizedDeltaRotator(
			OwnerInst->GetBaseAimRotation(), OwnerInst->GetActorRotation());
		const float TargetAimPitch = FMath::Clamp(AimDelta.Pitch, AimPitchMin, AimPitchMax);
		AimPitch = FMath::FInterpTo(AimPitch, TargetAimPitch, DeltaSeconds, AimPitchInterpSpeed);
		#pragma endregion

		FVector Velocity = OwnerInst->GetVelocity();
		FallSpeed = Velocity.Z;
		bIsFalling = OwnerInst->GetCharacterMovement()->IsFalling();
		bIsCrouching = OwnerInst->GetCharacterMovement()->IsCrouching();
		
		
		Velocity.Z = 0.f;
		MoveSpeed = Velocity.Size2D();
		
		if (!Velocity.IsNearlyZero())
		{
			const FRotator VelocityRotation = Velocity.ToOrientationRotator();
			const FRotator ActorRotation = OwnerInst->GetActorRotation();
			Direction = FRotator::NormalizeAxis(VelocityRotation.Yaw - ActorRotation.Yaw);
		}
	}
}
