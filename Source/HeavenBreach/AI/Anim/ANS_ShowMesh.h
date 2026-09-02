// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_ShowMesh.generated.h"

/**
 * 이미 캐릭터에 있는 메쉬를 키고 끄는 노티파이
 */
UCLASS()
class HEAVENBREACH_API UANS_ShowMesh : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 이 Tag가 붙은 컴포넌트를 켜고 끈다. 쓰고자하는 컴포넌트에 태그를 부착할 것
	UPROPERTY(EditAnywhere, Category = "Show")
	FName ComponentTag = TEXT("Arrow");

	// true  구간 동안 보이고 끝나면 숨긴다  (화살을 꺼내 쏘는 경우)
	// false 구간 동안 숨기고 끝나면 보인다  (등에 멘 무기를 뽑는 동안 숨기는 경우)
	UPROPERTY(EditAnywhere, Category = "Show")
	bool bVisibleDuringWindow = true;

private:
	void SetTaggedVisibility(USkeletalMeshComponent* MeshComp, bool bVisible) const;
};
