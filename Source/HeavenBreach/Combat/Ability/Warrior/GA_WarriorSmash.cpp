// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/Warrior/GA_WarriorSmash.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

UGA_WarriorSmash::UGA_WarriorSmash()
{
	// 방패 강타는 C++ 기본값으로 켠다. BP 에서 비워 두면 그 값이 이긴다.
	ShieldAugmentTag = WarriorTag::TAG_Augment_Smash_Shield;
	ShieldPowerEffect = UGE_WarriorSmashShieldPower::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Efficiency = Augments.AddDefaulted_GetRef();
	Efficiency.AugmentTag = WarriorTag::TAG_Augment_Smash_Efficiency;
	Efficiency.DisplayName = NSLOCTEXT("WarriorSmash", "EfficiencyName", "강타: 효율");
	Efficiency.Description = NSLOCTEXT("WarriorSmash", "EfficiencyDesc", "강타의 재사용 대기시간과 마나 소모가 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_ManaCost, 0.85f);
	Efficiency.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	// 강타 피해는 몽타주의 근접 판정이 정한다. 그 판정이 스킬 사용 중이면
	// SkillDamageMultiplier 를 곱하므로, 발동 중에만 그 배율을 올린다.
	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = WarriorTag::TAG_Augment_Smash_Damage;
	Power.DisplayName = NSLOCTEXT("WarriorSmash", "PowerName", "강타: 위력");
	Power.Description = NSLOCTEXT("WarriorSmash", "PowerDesc", "강타의 피해가 늘어납니다. (1단계 15%, 2단계 30%, 3단계 50%)");
	Power.TierMagnitudes = { 0.15f, 0.30f, 0.50f };
	Power.DuringEffect = UGE_WarriorSmashPowerAugment::StaticClass();

	FHBSkillAugment& Shield = Augments.AddDefaulted_GetRef();
	Shield.AugmentTag = WarriorTag::TAG_Augment_Smash_Shield;
	Shield.DisplayName = NSLOCTEXT("WarriorSmash", "ShieldName", "강타: 방패 강타");
	Shield.Description = NSLOCTEXT("WarriorSmash", "ShieldDesc", "강타를 쓰는 동안 방어력의 일부가 공격력에 더해집니다. (1단계 50%, 2단계 75%, 3단계 100%)");
	Shield.Params.Add(SkillParamTag::TAG_SkillParam_ShieldRatio, 0.5f);
	Shield.TierMagnitudes = { 0.5f, 0.75f, 1.0f };
}

UAnimMontage* UGA_WarriorSmash::GetMontageToPlay() const
{
	// 방패 강타 몽타주로 바꿔야 하는지 판단한다. 셋이 다 맞아야 바꾼다.
	//   1. 방패 강타 몽타주가 애셋에 지정되어 있고
	//   2. 어떤 증강이 방패 강타로 바꾸는지 태그가 지정되어 있고
	//   3. 그 증강을 실제로 골랐다
	// 하나라도 아니면 기본 몽타주를 그대로 쓴다.
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

	// 이 함수는 베이스의 ActivateAbility 안에서 불리므로 CurrentActorInfo 가 유효하다.
	// 그래서 HasAugment 의 편의형을 쓸 수 있다.
	return ShieldMontage;
}

void UGA_WarriorSmash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 베이스가 비용 부족이나 몽타주 없음으로 이미 끝냈으면 아무것도 하지 않는다.
	if (!IsActive())
	{
		return;
	}

	// 아래는 전부 방패 강타 증강 전용이다. 증강이 없으면 여기서 끝난다.
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

	// 방어력의 몇 배를 공격력에 얹을지. 증강 애셋의 Params 에 적혀 있다.
	// 배율이 아니라 전환 비율이라 미보유 기본값이 0 이다. 1.0 을 주면
	// 증강이 없는데도 방어력이 통째로 공격력에 더해진다.
	const float Ratio = GetAugmentParam(SkillParamTag::TAG_SkillParam_ShieldRatio, 0.f);

	// 방어력은 어트리뷰트라 GE 애셋에 숫자로 박아 둘 수 없다.
	// 발동 시점에 읽어서 SetByCaller 로 넘긴다. 그래야 장비를 바꾸면 같이 변한다.
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
	// 취소로 끝나도 반드시 걷는다. 안 떼면 공격력 보정이 영구히 남는다.
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
