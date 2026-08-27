// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnemyMontageGABase.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UEnemyMontageGABase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEnemyMontageGABase();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	// 쿨타임적용을 위해 함수 오버라이드
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// getter. 가중치
	float GetSelectionWeight() const { return SelectionWeight; }

	// 사거리에 맞는 어빌리티인지 확인
	bool IsRange(float Distance) const;
protected:
	// 실행시킬 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TObjectPtr<UAnimMontage> Montage;

	// 몽타주 실행 속도(공격속도)
	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	// 가중치. 높을수록 해당 어빌리티발동확률을 우선시하도록한다.
	// 자체적으로 해주는거아님. 어빌리티사용자가 판단해야함.
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	float SelectionWeight = 1.f;

	// 이 거리보다 가까우면 후보에서 제외
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	float MinRange = 0.f;

	// 이 거리보다 멀면 후보에서 제외. 0 이면 제한 없음
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	float MaxRange = 0.f;

	// 공격속도에 영향
	bool bIsAttackSpeed = true;

	// 쿨다운 적용방식
	// false : 즉시 발동. true : 몽타주 종료시
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	bool bCooldownOnEnd = false;

	// 몽타주와 함께 적용할 GE
	// 경직시간 추가하려고 만듬. 대상에게 GE부여는 몽타주 안 애님노티파이에서 써야한다. 이거아님
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UGameplayEffect> ActivationEffect;

	// 후딜레이 GE
	// 해당 상태동안 적은 다음 행동을 하지않음.
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UGameplayEffect> RecoveryEffect;

	// 몽타주가 끝남
	UFUNCTION()
	void OnMontageFinished();

	// 몽타주 중단
	UFUNCTION()
	void OnMontageCancelled();

private:
	// 쿨다운 적용중(종료시 쿨다운 적용 용도)
	bool bCooldownPending = false;
};
