// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/AN_StartRagdoll.h"

#include "Components/SkeletalMeshComponent.h"
#include "AI/EnemyCharacterBase.h"

void UAN_StartRagdoll::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(MeshComp->GetOwner());
	if (!Enemy)
	{
		return;
	}

	Enemy->StartRagdoll();
}
