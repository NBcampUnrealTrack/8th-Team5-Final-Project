// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Attack/GA_PlayerAttackRE.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"

UGA_PlayerAttackRE::UGA_PlayerAttackRE()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(StateTag::TAG_State_Attacking);

	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);

	// AssetTags 는 여기서 설정하지 않는다.
}

void UGA_PlayerAttackRE::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!AM_Attack)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	const float ManaCost = Player ? Player->ModifyManaCost(Player->GetEquippedWeaponTag(), Player->GetAttackManaCost()) : 0.f;

	// 클라 판정을 믿지 않는다. 재검증
	if (Player && !Player->HasEnoughMana(ManaCost))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 어트리뷰트는 서버 권위다. 내부에서 HasAuthority 로 한 번 더 막는다.
	if (Player)
	{
		Player->ConsumeMana(ManaCost);
	}

	// 어빌리티 발동
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 공격속도 적용
	float AttackRate = ASC ? FMath::Max(0.1f, ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute())) : 1.f;

	// 판정은 몽타주의 ANS_MeleeHitDetect 가 담당한다.
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AM_Attack, AttackRate);

	MontageTask->OnCompleted.AddDynamic(this, &UGA_PlayerAttackRE::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_PlayerAttackRE::OnMontageCompleted);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_PlayerAttackRE::OnMontageCancelled);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_PlayerAttackRE::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	// 이동 감속. 다른 버프와 곱해지도록 GE 로 건다.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (ASC && AttackSlowEffect)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AttackSlowEffect, 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Attack] 어빌리티 발동"));
}

void UGA_PlayerAttackRE::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveAttackSlow(ActorInfo);


	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_PlayerAttackRE::OnMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_PlayerAttackRE::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_PlayerAttackRE::RemoveAttackSlow(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!SlowHandle.IsValid())
	{
		return;
	}

	// 슬로우 해제.
	if (UAbilitySystemComponent* OwnerASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		OwnerASC->RemoveActiveGameplayEffect(SlowHandle);
	}
	SlowHandle.Invalidate();
}
