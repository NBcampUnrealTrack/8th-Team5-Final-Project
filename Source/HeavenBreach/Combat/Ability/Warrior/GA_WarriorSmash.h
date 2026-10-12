// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_WarriorSmash.generated.h"

/**
 * 강타. 증강 셋(효율·위력·방패 강타)은 생성자에 있다.
 * 베이스와 다른 동작은 증강 Shield 하나뿐이다.
 *   몽타주가 방패 강타로 바뀐다
 *   방어력의 일정 비율만큼 공격력이 오른다
 * 효율은 베이스가 Params 로, 위력은 베이스가 DuringEffect 로 처리한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_WarriorSmash : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_WarriorSmash();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 증강 Shield 를 보유하면 이 몽타주로 바꾼다.
	virtual UAnimMontage* GetMontageToPlay() const override;

	// ================ 애셋에서 지정 ================

	// 방패 강타로 바꾸는 증강. 예) Augment.Warrior.Smash.Shield
	UPROPERTY(EditDefaultsOnly, Category = "Smash")
	FGameplayTag ShieldAugmentTag;

	// 위 증강 보유 시 재생할 몽타주. 비우면 기본 몽타주를 그대로 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "Smash")
	TObjectPtr<UAnimMontage> ShieldMontage;

	// 위 값을 공격력에 더하는 GE. Player_AttackPower Additive SetByCaller.Damage.
	UPROPERTY(EditDefaultsOnly, Category = "Smash")
	TSubclassOf<UGameplayEffect> ShieldPowerEffect;

private:
	// 이번 발동에서 붙인 방패 보정. 종료 시 떼려고 들고 있는다.
	FActiveGameplayEffectHandle ShieldPowerHandle;
};
