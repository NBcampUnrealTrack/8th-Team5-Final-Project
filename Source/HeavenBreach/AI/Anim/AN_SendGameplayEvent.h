// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "GameplayTagContainer.h"
#include "AN_SendGameplayEvent.generated.h"

/**
 * 해당 태그의 이벤트를 호출하여 구독자들이 사용하도록 만듬
 */
UCLASS()
class HEAVENBREACH_API UAN_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
private:
	UPROPERTY(EditAnywhere, Category = "GameAbility")
	FGameplayTag EventTag;
};
