// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MoveForward.generated.h"

/**
 * 구간 동안 정면으로 이동시킨다. 대시 공격과 백스텝이 쓴다.
 * 해당 Notify를 사용하고자 한다면 해당 애니메이션의 Enable Root Motion 은 꺼둘 것
 */
UCLASS()
class HEAVENBREACH_API UANS_MoveForward : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 초당 이동 거리. 음수면 후진한다. (백스텝)
	UPROPERTY(EditAnywhere, Category = "Move")
	float Speed = 300.f;

	// 수평으로만 이동한다. 끄면 정면 벡터의 Z 를 그대로 따라간다.
	//
	// 왜 남겨두나: 비행 적이 생기면 정면이 기울어진다. 그때 이 값이 필요하다.
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bHorizontalOnly = true;

	// 대시 구간의 가속도. 기본 가속도로는 짧은 구간 안에 목표 속도에 도달하지 못해
	// 대시가 물렁해진다. 크게 잡아 한 프레임 만에 붙게 한다.
	//
	// 쳐낼 조건: 서서히 빨라지는 돌진을 원하면 이 값을 낮춘다.
	UPROPERTY(EditAnywhere, Category = "Move", meta = (ClampMin = "0.0"))
	float DashAcceleration = 100000.f;
};
