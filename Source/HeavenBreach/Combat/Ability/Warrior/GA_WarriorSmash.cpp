// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/Warrior/GA_WarriorSmash.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

UAnimMontage* UGA_WarriorSmash::GetMontageToPlay() const
{
	// 증강에 따라 방패강타 몽타주로 변경해야할 수 있음.
	// 1. 방패강타 몽타주가 잇어야하며 2. 방패 강타 증강이 어떤 태그인지 지정을 해야하며 3. 해당 증강 태그가 지금 부여되어잇는지 본다.
	if (!ShieldMontage)
	{
		return Super::GetMontageToPlay();
	}
	if (!ShieldAugmentTag.IsValid())
	{
		return Super::GetMontageToPlay();
	}
	if (!HasAugment(ShieldAugmentTag))
	{
		return Super::GetMontageToPlay();
	}

	// 방패 강타로 교체. 베이스의 ActivateAbility 안에서 불리므로 CurrentActorInfo 가 유효하다.
	return ShieldMontage;
}

void UGA_WarriorSmash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 베이스가 비용 부족이나 몽타주 없음으로 이미 끝냈으면 아무것도 붙이지 않는다.
	if (!IsActive())
	{
		return;
	}

	// 이후의 것들은 방패 강타 증강요소들이다. 증강이없다면 스킵
	if (!ShieldAugmentTag.IsValid())
	{
		return;
	}
	if (!HasAugment(ShieldAugmentTag))
	{
		return;
	}
	if (!ShieldPowerEffect)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	// 환산 비율은 증강 데이터의 Value 다. GA 에 두면 증강 설명문과 숫자가 갈린다.
	// HasAugment 로 이미 걸렀으므로 미보유 시 1.0 이 오는 문제는 없다.
	const float Ratio = GetAugmentValue(ShieldAugmentTag);

	// 방어력은 어트리뷰트라 GE 의 고정 수치로 표현할 수 없다.
	// 지금 값을 읽어 SetByCaller 로 넘긴다. 발동 순간의 방어력으로 고정된다.
	const float Defense = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute());
	const float BonusPower = Defense * Ratio;
	if (BonusPower <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, ShieldPowerEffect, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Damage, BonusPower);
	ShieldPowerHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UGA_WarriorSmash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소로 끝나도 공격력 보정이 남으면 안 된다. 제거
	if (ShieldPowerHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetASC(ActorInfo))
		{
			ASC->RemoveActiveGameplayEffect(ShieldPowerHandle);
		}
		ShieldPowerHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}