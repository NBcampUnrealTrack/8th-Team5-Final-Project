// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "HBPlayerActiveGABase.generated.h"

class AHBPlayerCharacter;
class UAbilitySystemComponent;
class UAnimMontage;
class UGameplayEffect;

/**
 * 플레이어 액티브 공통 GA
 * 비용,쿨타임, 몽타주 재생, 종료, 증강 까지 한다.
 * 판정은 몽타주의 ANS, 벞 디벞은 GE
 */
UCLASS()
class HEAVENBREACH_API UHBPlayerActiveGABase : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHBPlayerActiveGABase();

	// 오버라이드 시킨 GA함수들
	// 
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// UI 가 DA 대신 여기서 읽는다. 값의 주인은 GA 애셋 하나여야 한다.
	// 증강 반영 전의 기본값이고, 증강 전 수치 를 표시할 때도 쓴다.
	UFUNCTION(BlueprintPure, Category = "Skill")
	float GetBaseCooldown() const { return Cooldown; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	float GetBaseManaCost() const { return ManaCost; }

	// 재생할 몽타주. 자식 클래스에서 증강에따라 몽타주가변경된다면? 이거로 확인
	UFUNCTION(BlueprintPure, Category = "Skill")
	float GetBaseBuffDuration() const { return BuffDuration; }

	// 증강 깔때기에 넘길 출처 태그. 캐릭터가 UI가 알아보는데 쓴다.
	UFUNCTION(BlueprintPure, Category = "Skill")
	FGameplayTag GetSourceTag() const { return SourceTag; }

	virtual UAnimMontage* GetMontageToPlay() const { return Montage; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ================ 애셋에서 지정 ================

	// 스킬, 증강 조회에 쓰는 출처 태그. 증강 루트를 넣는다. 예) Augment.Warrior.BattleCry
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	FGameplayTag SourceTag;

	// 재생될 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TObjectPtr<UAnimMontage> Montage;

	// 쿨다운 GE 의 지속시간. GE 자체는 Cooldowns 의 CooldownGameplayEffectClass 에 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill", meta = (ClampMin = "0.0"))
	float Cooldown = 0.f;

	// 코스트 GE 의 마나 소비량. GE 자체는 Costs 의 CostGameplayEffectClass 에 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill", meta = (ClampMin = "0.0"))
	float ManaCost = 0.f;

	// 사망, 피격 몽타주까지 공격속도를 따라가면 빨라지거나 느려진다. 공격 어빌리티면 true.
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	bool bUseAttackSpeed = true;

	// ---- 무기 제약 ----

	// 이 무기를 들어야 발동한다. 비우면 제약 없음. Weapon 태그만 넣으면 아무 무기나 가능.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Requirement")
	FGameplayTagContainer RequiredWeaponTags;

	// 이 태그를 보유 중이면 무기 제약을 무시한다. 무기의 달인.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Requirement")
	FGameplayTag WeaponBypassTag;

	// ---- 자기 버프 ----

	// 비우면 안 붙인다. 전투의 함성, 피의 광란 같은 자버프 계열 GE.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff")
	TSubclassOf<UGameplayEffect> SelfBuffEffect;

	// 자버프 GE 지속시간. SetByCaller.Duration 으로 넘긴다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff", meta = (ClampMin = "0.0"))
	float BuffDuration = 0.f;

	// 증강이 있으면 GE 를 하나 더 붙인다. 함성.Damage 같은 4곳이 이 모양이다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> AugmentBuffEffects;

	// 버프 지속시간을 늘리는 증강. 비우면 추가 배율 없음.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff")
	FGameplayTag BuffDurationAugmentTag;

	// ================ ActorInfo 를 직접 받는 것 ================
	// 위 오버라이드 넷에서 쓴다. CurrentActorInfo 가 아직 없는 시점이라 인자를 써야 한다.

	// ActorInfo 에서 ASC 를 꺼낸다. 쓰이는 곳이 많아 널 검사를 여기 모았다.
	UAbilitySystemComponent* GetASC(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 캐릭터에 공격, 방어, 달리기 자원이 GAS 밖에 있어서 캐릭터로 접근해야 한다.
	AHBPlayerCharacter* GetPlayerFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 증강의 마나 감소가 반영된 값. CheckCost 와 ApplyCost 가 같은 값을 봐야 한다.
	float GetFinalManaCostFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 증강의 쿨타임 감소가 반영된 값.
	float GetFinalCooldownFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 기본 공격은 장착 무기에 따라 출처가 달라지므로 오버라이드한다.
	virtual FGameplayTag GetSourceTagFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// ================ 발동 중에만 쓰는 것 ================
	// 파생 클래스와 블루프린트용. ActivateAbility 이후에는 CurrentActorInfo 가 유효하다.

	UFUNCTION(BlueprintPure, Category = "Skill")
	AHBPlayerCharacter* GetPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAugment(FGameplayTag AugmentTag) const;

	// 증강 배율. 배율 미획득이면 1.0
	// 지속시간, 도약거리, 흡수량 같은 스킬 고유 수치가 이걸 쓴다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	float GetAugmentValue(FGameplayTag AugmentTag) const;

	// 파생이 버프를 조기에 걷어야 할 때 부른다. 돌격의 공중 피해감소 등.
	void RemoveSelfBuff();

private:
	void ApplySelfBuff(const FGameplayAbilityActorInfo* ActorInfo);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	// 이번 발동에서 붙인 버프 GE 핸들. 조기 제거용으로만 들고 있는다.
	TArray<FActiveGameplayEffectHandle> BuffHandles;
};
