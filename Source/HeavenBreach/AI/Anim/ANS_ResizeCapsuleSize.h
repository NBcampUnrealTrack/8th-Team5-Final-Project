// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_ResizeCapsuleSize.generated.h"

/**
 * 보스가 도약공격을 하는데 무릎을 접고 뛰는 도약 애니메이션에 따라 충돌크기를 줄여야 할 애니메이션이라 제작된 노티파이
 * 캡슐의 위치는 건드리지않고 크기만 건들인다.
 */
UCLASS()
class HEAVENBREACH_API UANS_ResizeCapsuleSize : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 높이(세로)에 곱할 값.
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HeightScale = 0.5f;

	// 반경(가로)에 곱할 값.
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RadiusScale = 1.0f;

	// 안정장치. 아직 
	UPROPERTY(EditAnywhere, Category = "Capsule")
	bool bRestoreOnLanded = true;

private:
	void ApplySize(USkeletalMeshComponent* MeshComp, float InHeightScale, float InRadiusScale) const;
};
