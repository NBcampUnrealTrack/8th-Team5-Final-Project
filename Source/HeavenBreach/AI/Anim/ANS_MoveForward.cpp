// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_MoveForward.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

void UANS_MoveForward::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || FMath::IsNearlyZero(Speed))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// 서버, 또는 자기 캐릭터를 조작하는 클라이언트만 움직인다.
	// 시뮬레이트 프록시가 스스로 움직이면 복제된 위치와 싸워 떨린다.
	const APawn* Pawn = Cast<APawn>(Owner);
	const bool bLocallyControlled = Pawn && Pawn->IsLocallyControlled();
	if (!Owner->HasAuthority() && !bLocallyControlled)
	{
		return;
	}

	FVector Direction = Owner->GetActorForwardVector();
	if (bHorizontalOnly)
	{
		Direction.Z = 0.f;
		Direction = Direction.GetSafeNormal();
	}

	if (Direction.IsNearlyZero())
	{
		return;
	}

	// 벽에 닿으면 스윕이 멈춰준다.
	FHitResult Hit;
	Owner->AddActorWorldOffset(Direction * Speed * FrameDeltaTime, bSweep, &Hit);
}
