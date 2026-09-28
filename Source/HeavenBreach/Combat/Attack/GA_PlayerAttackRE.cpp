// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Attack/GA_PlayerAttackRE.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "EquipmentComponent.h"

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

	// 경직 중에는 공격이 나가지 않는다.
	// 작동방식: 피격 GE 나 노티파이가 State.Staggered 를 붙이면
	// CanActivateAbility 가 이 목록과 비교해 발동을 거부한다.
	// State.HitReact 는 넣지 않는다. 그 태그는 "행동 유지" 용도로 나뉘어 있다.
	// State.Grabbed 도 넣지 않는다. 잡힌 상태에서 발버둥칠 수 있어야 한다는 기획이다.
	// 쳐낼 조건: 경직 중 공격으로 반격하는 시스템이 생기면 이 줄을 뺀다.
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);

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

	// 장착된 무기가 파손 상태면 애초에 공격을 시작하지 않는다(마나도 소모 안 함).
	if (Player)
	{
		if (const UEquipmentComponent* Equipment = Player->GetEquipment())
		{
			if (Equipment->IsEquippedItemBroken(EEquipSlotType::PrimaryWeapon))
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
				return;
			}
		}
	}

	// 비용 출처는 일반공격이다. 전투 호흡이 이 태그를 보고 마나를 깎는다.
	const float ManaCost = Player ? Player->ModifyManaCost(AbilityTag::TAG_Ability_BasicAttack, Player->GetAttackManaCost()) : 0.f;

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

	// 타격 성질을 실어 보낸다. 큐는 맞는 쪽이 쏘면서 이 태그를 읽는다.
	ApplyImpactCueTag(ActorInfo, Player);

	UE_LOG(LogTemp, Warning, TEXT("[Attack] Activate  Auth=%d  Local=%d"),
		ActorInfo->IsNetAuthority() ? 1 : 0,
		ActorInfo->IsLocallyControlled() ? 1 : 0);
}

void UGA_PlayerAttackRE::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveAttackSlow(ActorInfo);
	RemoveImpactCueTag(ActorInfo);

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

void UGA_PlayerAttackRE::ApplyImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo, AHBPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	// 이전 발동에서 안 떨어진 것이 있으면 먼저 정리한다. 겹치면 둘 다 남는다.
	RemoveImpactCueTag(ActorInfo);

	AppliedImpactCueTag = Player->GetImpactCueTag();
	if (AppliedImpactCueTag.IsValid())
	{
		// 루즈 태그를 쓰는 이유: 복제가 필요 없다. 큐를 쏘는 HitCuePlay::PlayImpact 는
		// 서버에서만 돌고, 이 어빌리티는 LocalPredicted 라 서버에서도 함께 붙는다.
		ASC->AddLooseGameplayTag(AppliedImpactCueTag);
	}
}

void UGA_PlayerAttackRE::RemoveImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!AppliedImpactCueTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveLooseGameplayTag(AppliedImpactCueTag);
	}
	AppliedImpactCueTag = FGameplayTag();
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
