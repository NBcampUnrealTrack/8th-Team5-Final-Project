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
	bComboWindowOpen = false;
	bComboWindowPassed = false;
	bComboInputQueued = false;
	PlayingMontage = nullptr;

	if (ComboMontages.IsEmpty() && !AM_Attack)
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

	// 일반공격은 마나를 쓰지 않는다. 마나는 달리기·방어·스킬에만 쓴다.

	// 어빌리티 발동
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 커밋 뒤에 고른다. 앞에서 실패한 발동이 콤보 번호를 넘기면 안 된다.
	UAnimMontage* Montage = PickMontage();
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	PlayingMontage = Montage;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 공격속도 적용
	float AttackRate = ASC ? FMath::Max(0.1f, ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute())) : 1.f;

	// 판정은 몽타주의 ANS_MeleeHitDetect 가 담당한다.
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, AttackRate);

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

	UE_LOG(LogTemp, Warning, TEXT("[Attack] Activate  Auth=%d  Local=%d  Montage=%s  NextCombo=%d"),
		ActorInfo->IsNetAuthority() ? 1 : 0,
		ActorInfo->IsLocallyControlled() ? 1 : 0,
		*GetNameSafe(Montage),
		ComboIndex);
}

void UGA_PlayerAttackRE::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		LastAttackEndTime = World->GetTimeSeconds();
	}
	bComboWindowOpen = false;
	bComboInputQueued = false;
	PlayingMontage = nullptr;

	RemoveAttackSlow(ActorInfo);
	RemoveImpactCueTag(ActorInfo);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UGA_PlayerAttackRE* UGA_PlayerAttackRE::FindActive(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		if (UGA_PlayerAttackRE* Attack = Cast<UGA_PlayerAttackRE>(Spec.GetPrimaryInstance()))
		{
			return Attack;
		}
	}
	return nullptr;
}

UAnimMontage* UGA_PlayerAttackRE::PickMontage()
{
	if (ComboMontages.IsEmpty())
	{
		return AM_Attack;
	}

	// 끝난 지 오래됐거나 마지막 타까지 쳤으면 1타부터.
	//
	// 서버와 클라가 각자 판단한다. 경계(딱 ComboResetDelay 근처)에서 누르면
	// 둘의 월드 시간 차이로 서로 다른 타를 고를 수 있다. 그 경우 판정 몽타주가 달라진다.
	// 쳐낼 조건: PIE 에서 실제로 어긋나는 게 보이면 클라가 고른 번호를 이벤트로 넘긴다.
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bExpired = LastAttackEndTime < 0.0 || Now - LastAttackEndTime > ComboResetDelay;
	if (bExpired || !ComboMontages.IsValidIndex(ComboIndex))
	{
		ComboIndex = 0;
	}

	UAnimMontage* Picked = ComboMontages[ComboIndex];
	++ComboIndex;
	return Picked;
}

void UGA_PlayerAttackRE::QueueComboInput()
{
	// 다음 타는 로컬이 발동해서 서버로 예측 전송한다. 서버 쪽 인스턴스는 기다리기만 한다.
	if (!IsLocallyControlled())
	{
		return;
	}

	// 구간이 이미 지나갔으면 이번 타로는 못 잇는다. 끝난 뒤 다시 누르면 ComboResetDelay 안이라 이어진다.
	if (bComboWindowPassed)
	{
		return;
	}

	bComboInputQueued = true;
	TryChainCombo();
}

void UGA_PlayerAttackRE::OpenComboWindow(const UAnimSequenceBase* Montage)
{
	if (Montage != PlayingMontage)
	{
		return;
	}

	bComboWindowOpen = true;
	TryChainCombo();
}

void UGA_PlayerAttackRE::CloseComboWindow(const UAnimSequenceBase* Montage)
{
	// 다음 타로 넘어가면 이전 몽타주가 멈추면서 NotifyEnd 가 늦게 온다.
	// 걸러내지 않으면 새로 시작한 타의 구간이 "지나갔음" 으로 잠긴다.
	if (Montage != PlayingMontage)
	{
		return;
	}

	bComboWindowOpen = false;
	bComboWindowPassed = true;
	bComboInputQueued = false;
}

void UGA_PlayerAttackRE::TryChainCombo()
{
	if (!bComboWindowOpen || !bComboInputQueued || !IsLocallyControlled())
	{
		return;
	}

	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!Player)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Combo] 다음 타로 넘어감  NextCombo=%d"), ComboIndex);

	// 끝내야 State.Attacking 이 떨어져 다음 발동이 막히지 않는다.
	// EndAbility 와 다음 발동 RPC 는 같은 ASC 채널로 순서대로 가므로 서버도 끝낸 뒤 받는다.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);

	// 사망·방어·무기 파손 검사를 다시 거치도록 입력 경로를 그대로 탄다.
	Player->Attack();
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
