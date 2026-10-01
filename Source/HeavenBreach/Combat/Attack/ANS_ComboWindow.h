// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_ComboWindow.generated.h"

/**
 * 평타 몽타주에서 "여기서부터 다음 타로 넘어갈 수 있다" 구간.
 *
 * 구간 안에 누르면 그 자리에서 다음 타가 나간다. 구간 전에 누른 입력은
 * 기억해 뒀다가 구간이 열리는 순간 쓴다. 구간이 지나간 뒤의 입력은 버린다.
 *
 * 왜 노티파이로 정하나: 시간값으로 박으면 애니메이션을 바꿀 때마다 코드를 고쳐야 한다.
 * 보통 타격 판정(ANS_MeleeHitDetect)이 끝난 뒤에 놓는다. 겹치면 판정이 끝나기 전에 끊긴다.
 */
UCLASS(meta = (DisplayName = "Combo Window"))
class HEAVENBREACH_API UANS_ComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
