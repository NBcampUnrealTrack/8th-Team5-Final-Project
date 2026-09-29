// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerAttackRE.generated.h"

/**
 * 근접 기본 공격. 무기 종류마다 BP를 하나씩 만들어 쓴다.
 * 어느 무기용인지는 BP 의 AssetTags 에 Weapon.타입 을 넣어 정한다.
 * 판정은 몽타주의 ANS_MeleeHitDetect 가 담당하므로 여기서는 재생만 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerAttackRE : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_PlayerAttackRE();

	// ASC 에서 지금 돌고 있는 평타를 찾는다. 없으면 nullptr.
	static UGA_PlayerAttackRE* FindActive(UAbilitySystemComponent* ASC);

	// 공격 중에 들어온 평타 입력. 구간이 열려 있으면 바로 다음 타로 넘어간다.
	void QueueComboInput();

	// ANS_ComboWindow 가 부른다. Montage 는 노티파이가 붙은 몽타주다.
	void OpenComboWindow(const UAnimSequenceBase* Montage);
	void CloseComboWindow(const UAnimSequenceBase* Montage);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageCancelled();

	// 단타 몽타주. ComboMontages 가 비어 있을 때만 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AM_Attack;

	// 콤보 순서대로 넣는다. 비어 있으면 AM_Attack 한 가지로 단타만 친다.
	// 다음 타로 넘어가는 구간은 각 몽타주의 ANS_ComboWindow 로 정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Combo")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

	// 공격이 끝난 뒤 이 시간 안에 다시 누르면 다음 타, 넘기면 1타부터.
	// 임시값이다. 손맛 보면서 맞춘다.
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Combo", meta = (ClampMin = "0.0", Units = "s"))
	float ComboResetDelay = 0.8f;

	// 공격중 이속감소 GE
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> AttackSlowEffect;

	// 사용중인 이속감소 GE의 핸들. 취소시켜야하니 들고있는다.
	FActiveGameplayEffectHandle SlowHandle;

	// 발동 중에만 붙여 둔 타격 성질 큐 태그. 끝날 때 떼려고 들고 있는다.
	FGameplayTag AppliedImpactCueTag;

	void RemoveAttackSlow(const FGameplayAbilityActorInfo* ActorInfo);

	// 장착 무기에 맞는 큐 태그를 붙이고 뗀다. 붙이는 곳과 떼는 곳을 한 쌍으로 둔다.
	void ApplyImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo, class AHBPlayerCharacter* Player);
	void RemoveImpactCueTag(const FGameplayAbilityActorInfo* ActorInfo);

private:
	// 이번 발동에 칠 몽타주를 고르고 콤보 번호를 한 칸 넘긴다.
	UAnimMontage* PickMontage();

	// 구간이 열려 있고 입력이 쌓여 있으면 지금 타를 끝내고 다음 타를 낸다.
	void TryChainCombo();

	// 다음에 칠 콤보 번호. InstancedPerActor 라 발동 사이에도 남는다.
	int32 ComboIndex = 0;

	// 마지막으로 공격이 끝난 월드 시간. 리셋 판단용.
	double LastAttackEndTime = -1.0;

	// 이번 발동이 재생 중인 몽타주. 이전 몽타주의 늦은 NotifyEnd 를 걸러낸다.
	UPROPERTY()
	TObjectPtr<const UAnimSequenceBase> PlayingMontage;

	bool bComboWindowOpen = false;
	bool bComboWindowPassed = false;
	bool bComboInputQueued = false;
};
