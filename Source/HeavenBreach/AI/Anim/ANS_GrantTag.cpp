// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_GrantTag.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_GrantTag::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !GrantedTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();

	if (!Owner)
		return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	if (!ASC)
		return;

	// 태그 등록
	ASC->AddLooseGameplayTag(GrantedTag);
}

void UANS_GrantTag::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	// 생각해보니 매틱할거없네...
}

void UANS_GrantTag::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !GrantedTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();

	if (!Owner)
		return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	if (!ASC)
		return;

	// 태그 해제
	ASC->RemoveLooseGameplayTag(GrantedTag);
}
