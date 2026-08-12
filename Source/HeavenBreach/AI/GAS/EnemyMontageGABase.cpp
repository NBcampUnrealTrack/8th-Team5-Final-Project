// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyMontageGABase.h"

#include "AI/Enum/EAlertState.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"

UEnemyMontageGABase::UEnemyMontageGABase()
{
	// 액터마다 하나씩
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// AI는 예측 실행이 필요없지않나? 서버에만
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 어차피 판단은 서버가 하니 클라의 요청을 전부 무시
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 해당 복제 옵션은 오너 클라에게만 복제? 애초에 주인자체가 서버하나뿐. 복제안함
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

void UEnemyMontageGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 이거 실행하려면 몽타주 잇어야함
	if (!Montage)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[GAS] %s Montage 미지정"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// CommitAbility선언은 필수
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// 맨끝의 저건 정상적으로 취소햇음? 취소·중단. 인터럽트, 조건 실패, 외부 취소여부
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 몽타주 재생을 어빌리티 태스크에 맡긴다
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, PlayRate);


	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 이벤트 바인딩
	Task->OnCompleted.AddDynamic(this, &UEnemyMontageGABase::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &UEnemyMontageGABase::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &UEnemyMontageGABase::OnMontageCancelled);
	Task->OnCancelled.AddDynamic(this, &UEnemyMontageGABase::OnMontageCancelled);

	// 이걸 빼먹으면 태스크가 시작되지 않는다
	Task->ReadyForActivation();

}

void UEnemyMontageGABase::OnMontageFinished()
{
	// 정상적으로 완료된것이니 false
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEnemyMontageGABase::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
