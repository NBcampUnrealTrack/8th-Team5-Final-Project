// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/AN_Bite.h"

#include "AI/GrabComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_Bite::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// 물기 시도. 잡은 게 없으면 컴포넌트가 알아서 무시한다.
	if (UGrabComponent* Grab = MeshComp->GetOwner()->FindComponentByClass<UGrabComponent>())
	{
		Grab->BiteGrabbed(DamageMultiplier);
	}
}
