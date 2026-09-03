// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_Grabbed.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_Grabbed::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UGrabComponent* Grab = GetGrabComponent(MeshComp);
	if (!Grab)
	{
		return;
	}

	// 잡기에 성공했으면 true 가 반환된다.
	if (Grab->TryGrab(GrabSettings))
	{
		return;
	}

	// 잡기 실패는 맞는데 다음 섹션으로 가려면 섹션 이름이 작성되어야 한다.
	if (FailSectionName.IsNone())
	{
		return;
	}

	// 실패했으니 끝나는 몽타주로 간다.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
	{
		ASC->CurrentMontageJumpToSection(FailSectionName);
	}
}

void UANS_Grabbed::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 구간이 끝났든 몽타주가 끊겼든 그랩을 해제한다.
	if (UGrabComponent* Grab = GetGrabComponent(MeshComp))
	{
		Grab->ReleaseGrab();
	}
}

UGrabComponent* UANS_Grabbed::GetGrabComponent(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return nullptr;
	}

	return Owner->FindComponentByClass<UGrabComponent>();
}
