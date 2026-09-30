// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/Attack/ANS_ComboWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Combat/Attack/GA_PlayerAttackRE.h"

namespace
{
	UGA_PlayerAttackRE* FindActiveAttack(const USkeletalMeshComponent* MeshComp)
	{
		// 시뮬레이티드 프록시는 활성 어빌리티가 없어 여기서 걸러진다.
		UAbilitySystemComponent* ASC = MeshComp
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner())
			: nullptr;
		return UGA_PlayerAttackRE::FindActive(ASC);
	}
}

void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UGA_PlayerAttackRE* Attack = FindActiveAttack(MeshComp))
	{
		Attack->OpenComboWindow(Animation);
	}
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UGA_PlayerAttackRE* Attack = FindActiveAttack(MeshComp))
	{
		Attack->CloseComboWindow(Animation);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UANS_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}
