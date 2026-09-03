// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AI/GrabComponent.h"
#include "ANS_Grabbed.generated.h"

/**
 * 잡고 있는 구간. 시작에서 잡고 끝에서 놓는다.
 * 사용하는 적은 그랩 컴포넌트가 필수로 존재해야함.
 */
UCLASS()
class HEAVENBREACH_API UANS_Grabbed : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 그랩 데이터 세팅
	UPROPERTY(EditAnywhere, Category = "Grab", meta = (ShowOnlyInnerProperties))
	FGrabSettings GrabSettings;

	// 잡기 실패시 건너뛸 섹션
	UPROPERTY(EditAnywhere, Category = "Grab")
	FName FailSectionName = TEXT("End");

private:
	// 서버가 아니면 nullptr. 판정과 GE 조작은 서버 전용이다.
	UGrabComponent* GetGrabComponent(USkeletalMeshComponent* MeshComp) const;
};
