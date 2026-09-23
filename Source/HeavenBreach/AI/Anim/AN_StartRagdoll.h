// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_StartRagdoll.generated.h"

/**
 * 사망 몽타주 끝에서 시체를 물리로 넘긴다.
 *
 * 왜 노티파이인가: 사망 처리(BecomeDead) 시점에 켜면 사망 애니가 아예 안 나오고
 * 그 자리에서 주저앉는다. "자세가 뜨기 시작하는 프레임" 은 애니메이터만 알 수 있어서
 * 타이밍을 몽타주로 넘긴다.
 *
 * 서버와 클라 각자에서 불린다. 스켈레탈 포즈는 복제되지 않으므로
 * 시체 자세가 클라마다 조금씩 다를 수 있다.
 */
UCLASS()
class HEAVENBREACH_API UAN_StartRagdoll : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
