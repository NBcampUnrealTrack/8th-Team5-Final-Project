// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/HBPlayerActiveGABase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "AbilitySystemGlobals.h"
#include "HBPlayerPassiveGABase.h"

UHBPlayerActiveGABase::UHBPlayerActiveGABase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 클라가 예측한다. 반응성을 위해서다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 상태 태그(State.Attacking 등)는 블루프린트의 ActivationOwnedTags 에 넣는다.
	// 스킬마다 다르므로 여기서 정하지 않는다. 반드시 BP_GA제작시 ActivationOwnedTags 추가할 것
}

// ---------------- ActorInfo 를 직접 받는 것 ----------------

UAbilitySystemComponent* UHBPlayerActiveGABase::GetASC(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}
	return ActorInfo->AbilitySystemComponent.Get();
}

AHBPlayerCharacter* UHBPlayerActiveGABase::GetPlayerFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	// ActorInfo에 있는 캐릭터 추출
	return Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
}

FGameplayTag UHBPlayerActiveGABase::GetSourceTagFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return SourceTag;
}

float UHBPlayerActiveGABase::GetFinalManaCostFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return ManaCost;
	}
	// 최종 마나 코스트 연산 반환
	return Player->ModifyManaCost(GetSourceTagFrom(ActorInfo), ManaCost);
}

float UHBPlayerActiveGABase::GetFinalCooldownFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return Cooldown;
	}
	// 최종 쿨타임 연산 반환
	return Player->ModifyCooldown(GetSourceTagFrom(ActorInfo), Cooldown);
}

// ---------------- 발동 중에만 쓰는 것 ----------------

AHBPlayerCharacter* UHBPlayerActiveGABase::GetPlayer() const
{
	// 플레이어 반환
	return GetPlayerFrom(GetCurrentActorInfo());
}

bool UHBPlayerActiveGABase::HasAugment(FGameplayTag AugmentTag) const
{
	const AHBPlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return false;
	}
	// 증강 목록 조회
	return Player->HasAugment(AugmentTag);
}

float UHBPlayerActiveGABase::GetAugmentValue(FGameplayTag AugmentTag) const
{
	const AHBPlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return 1.f;   // 미획득과 같은 값
	}
	// 증강으로 인한 지속시간 배율
	return Player->GetAugmentValue(AugmentTag);
}

// ---------------- 발동 판정 ----------------

bool UHBPlayerActiveGABase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 발동이 가능한지 확인
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 필요 태그 확인(필요 무기 제약이 비어있다면 제약따윈없음)
	if (RequiredWeaponTags.IsEmpty())
	{
		return true;   // 무기 제약 없음
	}

	// 당연하지만 플레이어에게만 동작한다.
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return false;
	}

	// 무기의 달인처럼 발동할 GA가 제약을 면제하는 태그를 갖고 있는지 본다.
	bool bBypassed = false;
	if (WeaponBypassTag.IsValid())
	{
		UAbilitySystemComponent* ASC = GetASC(ActorInfo);
		if (ASC)
		{
			bBypassed = ASC->HasMatchingGameplayTag(WeaponBypassTag);
		}
	}
	if (bBypassed)
	{
		return true;	// 제약조건 완화 덕에 통과
	}

	// 이제 실제 장비의 태그를 확인.
	const FGameplayTagContainer Equipped(Player->GetEquippedWeaponTag());
	if (!Equipped.HasAny(RequiredWeaponTags))
	{
		return false;	// 요구조건을 만족하는 장비가 아님.
	}

	// 요구 무기를 들고 있다.
	return true;
}

// ---------------- 비용 ----------------
// 비용만 Check 와 Apply 를 둘 다 오버라이드한다.
// 쿨타임은 CheckCooldown 이 태그 유무만 보므로 Apply 하나로 끝나지만,
// 비용은 CheckCost 가 GE 의 모디파이어를 실제로 평가하기 때문에 둘 다 필요하다.

bool UHBPlayerActiveGABase::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// GAS의 CheckCost는 CanApplyAttributeModifiers 로 GE의 모디파이어를 실제 평가하는데,
	// 그 안에서 스펙을 새로 만들기 때문에 우리가 ApplyCost 에서 넘길 SetByCaller 값을 모른다.
	// Super::로 사용되면 비용이 0 으로 평가돼 검사가 항상 통과한다.
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return false;
	}

	// 이 함수는 두 번 불린다. CanActivateAbility 에서 한 번, CommitCheck 에서 한 번.
	// 발동을 시작한 뒤 자원이 사라졌을 수 있어서 GAS 가 커밋 직전에 다시 본다.
	// 그리고 클라에서도 돌기 때문에, 마나가 부족하면 몽타주조차 나가지 않는다.
	if (Player->HasEnoughMana(GetFinalManaCostFrom(ActorInfo)))
	{
		return true;
	}

	// 실패 이유를 호출한 쪽에 남긴다. UI가 마나 부족 을 띄울때 이걸 본다.
	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(CostTag);
	}
	return false;
}

void UHBPlayerActiveGABase::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 부모함수를 부르지 않는다. 부모는 ApplyGameplayEffectToOwner 로 CDO 를 그대로 붙이기 때문에 SetByCaller 를 꽂을 틈이 없다.
	// 부르면 0 짜리 GE 가 한 번 더 붙으면서 "magnitude had not yet been set by caller" 경고만 남는다.
	if (!CostGameplayEffectClass)
	{
		return;   // 비용 없는 어빌리티
	}

	// 증강의 마나 감소가 여기서 반영된다. CheckCost 와 같은 값을 봐야 한다.
	const float FinalCost = GetFinalManaCostFrom(ActorInfo);
	if (FinalCost <= 0.f)
	{
		return;		// 코스트가 0이되면 할필요없지.
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,CostGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	// 증강의 마나 감소가 여기서 반영된다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_ManaCost, -FinalCost);

	// 활성화 예측 키로 붙기 때문에 클라 마나 바가 즉시 줄어든다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

// ---------------- 쿨타임 ----------------

void UHBPlayerActiveGABase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 증강이나 장비로 0 이 되면 지속시간 0짜리 GE가 붙어 쿨타임이 즉시 끝난다.
	const float FinalCooldown = GetFinalCooldownFrom(ActorInfo);
	if (FinalCooldown <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	// 증강의 쿨타임 감소가 여기서 반영된다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Cooldown, FinalCooldown);

	// 클라도 즉시 자기 쿨타임을 갖는다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

// ---------------- 발동 ----------------

void UHBPlayerActiveGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 여기서 ApplyCost 와 ApplyCooldown 이 예측 키로 붙는다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplySelfBuff(ActorInfo);

	// 몽타주 없는 스킬도 있다. 즉발 버프 같은 것.
	// 비용과 쿨타임과 버프는 CommitAbility로 이미 적용되었다.
	// 여기서 안 끝내면 몽타주가 null 이라 델리게이트가 하나도 안 오고
	// ActivationOwnedTags 가 남아 다음 발동이 막힌다.
	UAnimMontage* MontageToPlay = GetMontageToPlay();
	if (!MontageToPlay)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 공격속도 반영. 어트리뷰트는 1.0 이 기본이고 1.2 가 20% 증가다.
	float PlayRate = 1.f;
	if (bUseAttackSpeed)
	{
		UAbilitySystemComponent* ASC = GetASC(ActorInfo);
		if (ASC)
		{
			const float Rate = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute());
			PlayRate = FMath::Max(0.1f, Rate);
		}
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, PlayRate);
	if (!Task)
	{
		// AnimInstance 가 없으면 null 이 온다. 그냥 붙이면 크래시한다.
		UE_LOG(LogTemp, Warning, TEXT("[GA] %s 몽타주 태스크 생성 실패"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 네 경로 모두 종료로 묶는다. 하나라도 빠지면 어빌리티가 안 끝난다.
	Task->OnCompleted.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageCancelled);
	Task->OnInterrupted.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageCancelled);
	Task->ReadyForActivation();
}

// ---------------- 자기 버프 ----------------

void UHBPlayerActiveGABase::ApplySelfBuff(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo)
	{
		return;
	}

	// 버프는 서버가 붙이고 복제된다.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	// 증강으로 늘어난 지속시간. 배율을 먼저 만들어 곱셈 안에 삼항을 넣지 않는다.
	float DurationMultiplier = 1.f;
	if (BuffDurationAugmentTag.IsValid())
	{
		DurationMultiplier = GetAugmentValue(BuffDurationAugmentTag);
	}
	const float FinalDuration = BuffDuration * DurationMultiplier;

	if (SelfBuffEffect)
	{
		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(SelfBuffEffect, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Duration, FinalDuration);
			BuffHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}

	// 증강이 있으면 GE 를 하나 더 붙인다. 지속시간은 같이 간다.
	for (const TPair<FGameplayTag, TSubclassOf<UGameplayEffect>>& Pair : AugmentBuffEffects)
	{
		if (!Pair.Value)
		{
			continue;
		}
		if (!HasAugment(Pair.Key))
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(Pair.Value, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Duration, FinalDuration);
			BuffHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}
}

void UHBPlayerActiveGABase::RemoveSelfBuff()
{
	UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	if (!ASC)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& BuffHandle : BuffHandles)
	{
		if (!BuffHandle.IsValid())
		{
			continue;
		}
		ASC->RemoveActiveGameplayEffect(BuffHandle);
	}
	BuffHandles.Reset();
}

// ---------------- 종료 ----------------

void UHBPlayerActiveGABase::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UHBPlayerActiveGABase::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UHBPlayerActiveGABase::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility, bool bWasCancelled)
{
	// 버프의 지속시간은 어빌리티 수명보다 길다. 걷지 않고 핸들만 비운다.
	BuffHandles.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}