// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AI/GrabComponent.h"
#include "AN_GrabAttempt.generated.h"

class UGameplayEffect;

/**
 * [현재 해당 코드는 더미데이터입니다. 쓰지마세요] 
 * 그랩에 성공했는지 판단
 * 사용하는 적은 그랩 컴포넌트가 필수로 존재해야함
 */
UCLASS()
class HEAVENBREACH_API UAN_GrabAttempt : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;


protected:
	// 그랩 데이터 세팅
	UPROPERTY(EditAnywhere, Category = "Grab", meta = (ShowOnlyInnerProperties))
	FGrabSettings GrabSettings;

	// 잡기 실패시 건너뛸 섹션
	UPROPERTY(EditAnywhere, Category = "Grab")
	FName FailSectionName = TEXT("End");
};
