// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/HBPlayerPassiveGABase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Player/HBPlayerCharacter.h"

UHBPlayerPassiveGABase::UHBPlayerPassiveGABase()
{
	// InstancingPolicy 는 부모가 InstancedPerActor 로 정한다.

	// 패시브는 예측할 것이 없다. 태그와 GE 는 서버가 붙이고 클라로 복제된다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UHBPlayerPassiveGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 진단용 로그. 패시브가 언제 붙고 언제 떨어지는지 보려고 넣었다.
	// ActivationOwnedTags 가 발동 중 ASC 에 붙는 태그다. 여기가 비어 있으면
	// BP 에서 다른 칸(AssetTags 등)에 넣은 것이다.
	// 쳐낼 조건: 패시브 유지 문제가 정리되면 이 두 줄을 지운다.
	UE_LOG(LogTemp, Warning, TEXT("[Passive] %s 시작 | 부여태그=%s"),
		*GetName(), *ActivationOwnedTags.ToStringSimple());

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Passive] %s 커밋 실패 → 즉시 종료"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// EndAbility 를 일부러 부르지 않는다.
	// 어빌리티가 살아 있는 동안이 곧 패시브 보유 기간이고,
	// 그 동안 ActivationOwnedTags 가 ASC 에 유지된다.
	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	// 계산식이 있는 패시브만 GE 를 붙인다.
	// 태그만 필요한 패시브는 여기서 끝난다.
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
	// 진단용 로그. 패시브는 매치 내내 살아 있어야 하므로 이게 찍히면 뭔가 끊은 것이다.
	// 누가 끊었는지는 콜스택이 필요하다. 잦으면 여기에 브레이크포인트를 건다.
	// 쳐낼 조건: 패시브 유지 문제가 정리되면 이 두 줄을 지운다.
	UE_LOG(LogTemp, Warning, TEXT("[Passive] %s 종료 | 취소=%d"),
		*GetName(), bWasCancelled ? 1 : 0);

	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (ASC)
	{
		// 패시브가 끝나면 붙여 둔 GE 도 같이 걷는다.
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

// GetASC / GetPlayer / HasAugment / GetAugmentParam / FindAugment 는
// 부모(UHBPlayerSkillGABase)에 있다.
