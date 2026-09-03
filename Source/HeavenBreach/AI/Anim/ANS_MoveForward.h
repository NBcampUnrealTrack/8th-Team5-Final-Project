// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MoveForward.generated.h"

/**
 * 앞으로 전진하는 노티파이스테이트 
 * 해당 Notify를 사용하고자 한다면 해당 애니메이션의 Enable Root Motion 은 꺼둘 것
 */
UCLASS()
class HEAVENBREACH_API UANS_MoveForward : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 초당 이동 거리. 음수면 후진한다. (백스텝)
	UPROPERTY(EditAnywhere, Category = "Move")
	float Speed = 300.f;

	// 벽에 막히게 한다. 끄면 벽을 통과한다.
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bSweep = true;

	// 수평으로만 이동한다. 끄면 정면 벡터의 Z 를 그대로 따라간다.
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bHorizontalOnly = true;
};
