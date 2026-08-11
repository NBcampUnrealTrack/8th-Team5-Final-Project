#include "AN_MeleeHitCheck.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Core/Tag/Event/HB_EventTag.h"

void UAN_MeleeHitCheck::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                               const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!MeshComp)
	{
		return;
	}
	
	AActor* OwnerActor = MeshComp->GetOwner();
	
	if (!OwnerActor)
	{
		return;
	}
	
	FGameplayEventData EventData;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor,EventTag::TAG_Event_MeleeHitCheck,EventData);
}
