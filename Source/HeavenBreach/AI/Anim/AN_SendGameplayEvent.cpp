// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/AN_SendGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagsManager.h"
#include "AI/Enum/EAlertState.h"

void UAN_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp->GetOwner())
		return;

	UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());

	if (!OwnerASC)
		return;

	// 이벤트 발생 전달
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MeshComp->GetOwner(), EventTag, FGameplayEventData());

	UE_LOG(LogEnemyAI, Warning, TEXT("[Proj] 이벤트 발신 '%s'"), *EventTag.ToString());
}
