#include "HBPlayerAnimInst.h"

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
		Velocity.Z = 0.f;
		MoveSpeed = Velocity.Size2D();
	}
}
