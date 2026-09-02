// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_ShowMesh.h"

void UANS_ShowMesh::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	SetTaggedVisibility(MeshComp, bVisibleDuringWindow);
}

void UANS_ShowMesh::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	SetTaggedVisibility(MeshComp, !bVisibleDuringWindow);
}

void UANS_ShowMesh::SetTaggedVisibility(USkeletalMeshComponent* MeshComp, bool bVisible) const
{
	// 당연하지만 메쉬랑 태그는 있어야한다.
	if (!MeshComp || ComponentTag.IsNone())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// 태그로 컴포넌트를 검색한다. 태그가 여러개면 전부다 처리한다.
	TArray<UActorComponent*> Found = Owner->GetComponentsByTag(USceneComponent::StaticClass(), ComponentTag);
	for (UActorComponent* Comp : Found)
	{
		if (USceneComponent* Scene = Cast<USceneComponent>(Comp))
		{
			// 두 번째 인자는 자식까지 활성. 메쉬에 다른걸 달아둿따면 같이 따라간다.
			Scene->SetVisibility(bVisible, true);
		}
	}
}
