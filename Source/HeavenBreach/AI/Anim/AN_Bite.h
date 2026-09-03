// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_Bite.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UAN_Bite : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 공격 배율
	UPROPERTY(EditAnywhere, Category = "Bite", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 0.8f;
};
