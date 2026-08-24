// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyMontageGABase.h"

#include "AI/Enum/EAlertState.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "AI/GAS/EnemyAttributeSetBase.h"

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
		// 맨끝의 저건 정상적으로 취소햇음? 취소/중단. 인터럽트, 조건 실패, 외부 취소여부
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 당연하지만 ASC를 가지고 잇어야하며 서버만 가능
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 최종 몽타주 속도
	float FinalPlayRate = PlayRate;

	if (ASC)
	{
		// 공격속도 보정
		// 공격용 몽타주만 사용. 그외의 몽타주들은 기존 Rate적용
		if (bIsAttackSpeed)
		{
			bool bFound = false;
			float Rate = ASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetAttackSpeedRateAttribute(), bFound);

			if (bFound && Rate > 0.f)
			{
				FinalPlayRate = PlayRate * Rate;
			}
		}
	}


	// 몽타주와 별개로 지속되는 상태를 건다. 어빌리티가 끝나도 GE 는 남는다.
	if (ActivationEffect)
	{
		if (ASC && ASC->GetOwner() && ASC->GetOwner()->HasAuthority())
		{
			// 자기 자신에게 GE부여
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ActivationEffect, 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}



	// 몽타주 재생을 어빌리티 태스크에 맡긴다
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, FinalPlayRate);


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

bool UEnemyMontageGABase::IsRange(float Distance) const
{
	// 최소사거리보다 짧은지 판단한다.
	// 원거리 어빌리티의 경우 필요할수도있다.
	if (Distance < MinRange)
	{
		return false;
	}

	// MaxRange가 0일 시 사거리 제한없음
	if (MaxRange > 0.f && Distance > MaxRange)
	{
		return false;
	}

	return true;
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
