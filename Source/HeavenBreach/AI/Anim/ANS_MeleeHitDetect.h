// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MeleeHitDetect.generated.h"

class UGameplayEffect;

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UANS_MeleeHitDetect : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 히트판정 소켓들
	UPROPERTY(EditAnywhere, Category = "Trace")
	TArray<FName> TraceSockets;

	// 충돌체의 반지름
	UPROPERTY(EditAnywhere, Category = "Trace")
	float TraceRadius = 30.f;

	// 데미지 배율
	UPROPERTY(EditAnywhere, Category = "Trace")
	float DamageMultiplier = 1.f;

	// 히트스탑을 사용
	UPROPERTY(EditAnywhere, Category = "Trace")
	bool bUseHitStop = false;

	// 히트 시 느려지는것 지속시간
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "bUseHitStop", ClampMin = "0.0"))
	float HitStopDuration = 0.4f;

	// 히트 시 얼마나 느려질지
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopPlayRate = 0.05f;

	// 데미지 적용 GE
	// 트레이스에 충돌난 대상에게 적용
	UPROPERTY(EditAnywhere, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;
};
