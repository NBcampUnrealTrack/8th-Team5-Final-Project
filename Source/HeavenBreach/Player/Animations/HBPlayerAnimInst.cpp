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
		// 본인은 카메라가 머리 뼈에 붙어 있어 이 값이 곧 화면 각도다. 보간하면 화면이 마우스보다 늦게 따라온다.
		// 남은 복제된 조준값이 끊어 들어오므로 보간으로 부드럽게 한다.
		// 계산 방식은 4타 콤보 테스트(ABP_Test_ComboWarrior)와 같다.
		const bool bLocal = OwnerInst->IsLocallyControlled();
		const FRotator AimRotation = bLocal && OwnerInst->GetController()
			? OwnerInst->GetController()->GetControlRotation()
			: OwnerInst->GetBaseAimRotation();
		const FRotator AimDelta = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, OwnerInst->GetActorRotation());
		const float TargetAimPitch = FMath::Clamp(AimDelta.Pitch, AimPitchMin, AimPitchMax);
		AimPitch = bLocal ? TargetAimPitch : FMath::FInterpTo(AimPitch, TargetAimPitch, DeltaSeconds, AimPitchInterpSpeed);
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
