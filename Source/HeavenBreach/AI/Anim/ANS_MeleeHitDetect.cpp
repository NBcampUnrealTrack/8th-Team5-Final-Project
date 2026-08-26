// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Anim/ANS_MeleeHitDetect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AI/Enum/EAlertState.h"
#include "AI/MeleeTraceComponent.h"

void UANS_MeleeHitDetect::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp)) return;

	AActor* Owner = MeshComp->GetOwner();

	// 서버 전용
	if (!IsValid(Owner) || !Owner->HasAuthority()) 
		return;

	if (TraceSockets.Num() < 2)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[HIT] 소켓이 2개 미만이라 검사가 나가지 않는다"));
		return;
	}

	UMeleeTraceComponent* TraceComp = Owner->FindComponentByClass<UMeleeTraceComponent>();

	// 컴포넌트는 데이터 에셋에 넣도록 할건데 안 넣은 경우
	if (!TraceComp)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[HIT] %s 에 MeleeTraceComponent 없음"), *GetNameSafe(Owner));
		return;
	}

	// 소켓과 GE, 트레이스 크기와 데미지배율 삽입
	TraceComp->BeginTrace(TraceSockets, DamageEffect, TraceRadius, DamageMultiplier);

	// 시작하자마자 바로 트레이스. 너무짧아서 Tick이 호출되지 않는것을 방지.
	TraceComp->UpdateTrace();
}

void UANS_MeleeHitDetect::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!IsValid(MeshComp)) 
		return;

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) 
		return;

	// 컴포넌트에 업데이트 하라고 신호보낸다.
	if (UMeleeTraceComponent* TraceComp = Owner->FindComponentByClass<UMeleeTraceComponent>())
	{
		TraceComp->UpdateTrace();
	}
}

void UANS_MeleeHitDetect::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp)) 
		return;

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
		return;

	// 컴포넌트에 공격이 종료됨을 알림
	if (UMeleeTraceComponent* TraceComp = Owner->FindComponentByClass<UMeleeTraceComponent>())
	{
		TraceComp->EndTrace();
	}
}