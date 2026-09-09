// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/HBPlayerPassiveGABase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Player/HBPlayerCharacter.h"

UHBPlayerPassiveGABase::UHBPlayerPassiveGABase()
{
	// 핸들을 멤버로 들고 있으므로 인스턴스가 필요하다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 패시브는 예측할 게 없다. 태그와 GE 는 복제로 클라에 도달한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UHBPlayerPassiveGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// EndAbility 를 스스로 부르지 않는다.
	// 어빌리티가 살아있는 동안이 곧 패시브 보유 기간이고, 그동안 ActivationOwnedTags 가 유지된다.
	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	// 모디파이어가 있는 패시브만. 태그만 필요한 패시브는 여기서 끝난다.
	if (!SelfPassiveEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(SelfPassiveEffect, 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	PassiveHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
}

void UHBPlayerPassiveGABase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (ASC)
	{
		// 패시브 핸들로 GA제거
		for (const FActiveGameplayEffectHandle& PassiveHandle : PassiveHandles)
		{
			if (!PassiveHandle.IsValid())
			{
				continue;
			}
			ASC->RemoveActiveGameplayEffect(PassiveHandle);
		}
	}
	PassiveHandles.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAbilitySystemComponent* UHBPlayerPassiveGABase::GetASC(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}
	return ActorInfo->AbilitySystemComponent.Get();
}