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
		// 컨트롤러가 있으면(본인, 또는 서버) 정확한 조준값을 보간 없이 쓴다.
		// - 본인: 카메라가 머리 뼈에 붙어 있어 이 값이 곧 화면 각도다. 보간하면 화면이 마우스보다 늦다.
		// - 서버: 타격 판정(ANS_MeleeHitDetect)이 서버의 이 자세로 돈다. 서버는 클라의 컨트롤러를 갖고 있어
		//   정확한 값을 안다. 전에는 서버도 복제값+보간을 써서, 클라가 시야를 빠르게 움직이며 치면
		//   판정 자세가 화면보다 최대 47도 덜 숙여져 있었다(서버에서 두 값을 로그로 비교해 확인).
		// 컨트롤러가 없는 남의 화면만 복제된 조준값이 끊어 들어오므로 보간으로 부드럽게 한다.
		const AController* Controller = OwnerInst->GetController();
		const FRotator AimRotation = Controller ? Controller->GetControlRotation() : OwnerInst->GetBaseAimRotation();
		const FRotator AimDelta = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, OwnerInst->GetActorRotation());
		const float TargetAimPitch = FMath::Clamp(AimDelta.Pitch, AimPitchMin, AimPitchMax);
		AimPitch = Controller ? TargetAimPitch : FMath::FInterpTo(AimPitch, TargetAimPitch, DeltaSeconds, AimPitchInterpSpeed);
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
