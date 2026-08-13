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
	
	if (OwnerInst)
	{
		FVector Velocity = OwnerInst->GetVelocity();
		FallSpeed = Velocity.Z;
		bIsFalling = OwnerInst->GetCharacterMovement()->IsFalling();
		
		
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
