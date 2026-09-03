// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/AN_GrabAttempt.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/GrabComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_GrabAttempt::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// 판정은 서버에서만 해준다.
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	UGrabComponent* Grab = Owner->FindComponentByClass<UGrabComponent>();
	if (!Grab)
	{
		return;
	}

	// 잡기에 성공했으면 true가 반환된다.
	if (Grab->TryGrab(GrabSettings))
	{
		return;
	}

	// 잡기 실패는 맞는데 다음섹션으로 가려면 섹션 이름이 작성되어야 한다.
	if (FailSectionName.IsNone())
	{
		return;
	}

	// 실패했으니 마무리로 보낸다.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		ASC->CurrentMontageJumpToSection(FailSectionName);
	}
}
