// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_MoveForward.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/EnemyCharacterBase.h"

namespace
{
	// 노티파이가 다룰 수 있는 대상인지 한 번에 확인한다.
	// 셋 중 하나라도 없으면 아무것도 하지 않는다.
	AEnemyCharacterBase* ResolveMover(USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return nullptr;
		}

		AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(MeshComp->GetOwner());
		if (!Enemy)
		{
			return nullptr;
		}

		// AI 는 서버 권위다. 시뮬레이트 프록시가 스스로 움직이면 복제된 위치와 싸워 떤다.
		if (!Enemy->HasAuthority())
		{
			return nullptr;
		}

		if (!Enemy->GetCharacterMovement())
		{
			return nullptr;
		}

		return Enemy;
	}
}

void UANS_MoveForward::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AEnemyCharacterBase* Enemy = ResolveMover(MeshComp);
	if (!Enemy)
	{
		return;
	}

	UCharacterMovementComponent* Move = Enemy->GetCharacterMovement();

	// AddMovementInput 은 MaxWalkSpeed 를 넘지 못한다. 대시 속도를 상한으로 올려 둔다.
	// 뒤로 가는 구간도 거리는 크기로 정해지므로 절대값을 쓴다.
	Move->MaxWalkSpeed = FMath::Abs(Speed);
	Move->MaxAcceleration = DashAcceleration;
}

void UANS_MoveForward::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (FMath::IsNearlyZero(Speed))
	{
		return;
	}

	AEnemyCharacterBase* Enemy = ResolveMover(MeshComp);
	if (!Enemy)
	{
		return;
	}

	FVector Direction = Enemy->GetActorForwardVector();
	if (bHorizontalOnly)
	{
		Direction.Z = 0.f;
		Direction = Direction.GetSafeNormal();
	}

	if (Direction.IsNearlyZero())
	{
		return;
	}

	// 음수 Speed 는 뒤로 간다. 크기는 MaxWalkSpeed 가 이미 들고 있다.
	if (Speed < 0.f)
	{
		Direction = -Direction;
	}

	// CharacterMovement 에 입력을 준다. 경사·계단·벽 처리는 전부 여기서 해 준다.
	Enemy->AddMovementInput(Direction, 1.f);
}

void UANS_MoveForward::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AEnemyCharacterBase* Enemy = ResolveMover(MeshComp);
	if (!Enemy)
	{
		return;
	}

	// 속도는 폰이 다시 계산한다. 저장해 둔 값을 되돌리는 게 아니라서
	// 대시 도중에 이동속도 버프가 붙거나 걷기/달리기가 바뀌어도 맞는 값이 나온다.
	Enemy->RefreshMoveSpeed();

	// 가속도는 다시 계산할 근거가 없어 클래스 기본값에서 가져온다.
	// BP 에서 바꿨다면 그 값이 CDO 에 들어 있다.
	UCharacterMovementComponent* Move = Enemy->GetCharacterMovement();
	const ACharacter* DefaultChar = Enemy->GetClass()->GetDefaultObject<ACharacter>();
	const UCharacterMovementComponent* DefaultMove = DefaultChar ? DefaultChar->GetCharacterMovement() : nullptr;

	if (DefaultMove)
	{
		Move->MaxAcceleration = DefaultMove->MaxAcceleration;
	}

	// 대시가 끝나도 속도가 남아 미끄러진다. 여기서 끊는다.
	Move->StopMovementImmediately();
}
