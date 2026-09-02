// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_ResizeCapsuleSize.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UANS_ResizeCapsuleSize::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ApplySize(MeshComp, HeightScale, RadiusScale);
}

void UANS_ResizeCapsuleSize::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	//if (!bRestoreOnLanded || !MeshComp)
	//{
	//	return;
	//}
	//
	//const ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	//if (!Character || !Character->GetCharacterMovement())
	//{
	//	return;
	//}

	// 안전장치
	//if (!Character->GetCharacterMovement()->IsFalling())
	//{
	//	ApplySize(MeshComp, 1.0f, 1.0f);
	//}
}

void UANS_ResizeCapsuleSize::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ApplySize(MeshComp, 1.0f, 1.0f);
}

void UANS_ResizeCapsuleSize::ApplySize(USkeletalMeshComponent* MeshComp, float InHeightScale, float InRadiusScale) const
{
	if (!MeshComp)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!Character)
	{
		return;
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const ACharacter* DefaultCharacter = Character->GetClass()->GetDefaultObject<ACharacter>();
	if (!DefaultCharacter || !DefaultCharacter->GetCapsuleComponent())
	{
		return;
	}

	float BaseHalfHeight = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	float BaseRadius = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	float TargetHalfHeight = BaseHalfHeight * InHeightScale;
	float TargetRadius = BaseRadius * InRadiusScale;

	// 실제 변경되는 량
	float HeightDelta = TargetHalfHeight - Capsule->GetUnscaledCapsuleHalfHeight();

	Capsule->SetCapsuleSize(TargetRadius, TargetHalfHeight, true);

	// 땅에 붙은 채로 캡슐을 키우면 아랫면이 바닥을 파고드니 그만큼 올려준다. 공중에 있으면 발동x
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (HeightDelta > 0.f && Movement && !Movement->IsFalling())
	{
		Character->SetActorLocation(Character->GetActorLocation() + FVector(0.f, 0.f, HeightDelta));
	}
}
