// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "HBPlayerActiveGABase.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 플레이어 액티브 공통 GA
 * 비용,쿨타임, 몽타주 재생, 종료, 증강 까지 한다.
 * 판정은 몽타주의 ANS, 벞 디벞은 GE
 *
 * 증강 목록과 출처 태그, 아바타 접근은 부모(UHBPlayerSkillGABase)에 있다.
 * 패시브와 공유해야 하는 것이라 그리로 올렸다.
 */
UCLASS()
class HEAVENBREACH_API UHBPlayerActiveGABase : public UHBPlayerSkillGABase
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

	// UI 용 최종 마나 코스트(스킬 증강 SkillParam.ManaCost + 전역 보정). 발동 때 CheckCost·ApplyCost 와 같은 값이다.
	// CDO 에서 불러도 된다 — 플레이어는 인스턴스가 아니라 넘긴 ActorInfo 의 AvatarActor 에서 읽는다.
	float GetFinalManaCostForActor(const FGameplayAbilityActorInfo* ActorInfo) const { return GetFinalManaCostFrom(ActorInfo); }

	// 재생할 몽타주. 자식 클래스에서 증강에따라 몽타주가변경된다면? 이거로 확인
	UFUNCTION(BlueprintPure, Category = "Skill")
	float GetBaseBuffDuration() const { return BuffDuration; }

	// 설명 치환이 이 GE 의 모디파이어에서 수치를 읽는다.
	UFUNCTION(BlueprintPure, Category = "Skill")
	TSubclassOf<UGameplayEffect> GetSelfBuffEffect() const { return SelfBuffEffect; }

	virtual UAnimMontage* GetMontageToPlay() const { return Montage; }

	// 발동 중 좌클릭 누름/뗌을 이 스킬이 직접 받고 싶은가. 조준 스킬이 true 로 오버라이드한다.
	// 캐릭터는 State.Bow.SkillAiming 이 붙어 있고 이 값이 true 인 활성 스킬 하나에만 입력을 넘긴다.
	// 그 스킬은 UAbilityTask_WaitInputPress/Release 로 받고, 우클릭은 캐릭터가 CancelAbility 로 처리한다.
	virtual bool WantsAimInput() const { return false; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ================ 애셋에서 지정 ================

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
	//
	// Augments 배열과 헷갈리지 말 것.
	//   Augments           이 스킬에 어떤 증강이 있는가 (목록, 설명, 수치)
	//   AugmentBuffEffects 그중 가진 증강이 있을 때 발동 시 같이 붙일 버프 GE
	// 하나는 목록이고 하나는 실행이다. 증강 태그로 서로를 가리킨다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> AugmentBuffEffects;

	// ---- 발동 중에만 유지 ----

	// 발동 중에만 걸고 어빌리티가 끝나면 즉시 제거하는 GE.
	// 스킬 사용 중 이동 감속이 여기 들어간다.
	// 기본공격(GA_PlayerAttackRE)의 AttackSlowEffect 와 같은 역할이다.
	//
	// 작동방식: ActivateAbility 에서 붙이고 EndAbility 에서 뗀다.
	// 서버에서만 붙이고, 이동속도 어트리뷰트가 복제되면 클라가 따라온다.
	//
	// SelfBuffEffect 와 다른 점: 지속시간이 아니라 어빌리티 수명을 따른다.
	// 비우면 아무것도 걸지 않으므로, 감속이 필요 없는 스킬은 비워두면 된다.
	// GE 는 Infinite 여야 한다. 지속시간을 주면 어빌리티보다 먼저 풀린다.
	//
	// 쳐낼 조건: "발동 중에만" 걸 것이 두 종류 이상 생기면 배열로 바꾼다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Buff")
	TSubclassOf<UGameplayEffect> DuringAbilityEffect;

	// ---- 발동 연출 ----

	// 발동 순간 터뜨릴 GameplayCue. 비우면 아무것도 안 한다.
	//
	// 왜 몽타주 AnimNotify 가 아닌가: 몽타주를 공유하는 스킬끼리 연출이 섞인다.
	// AM_Warrior_Buff 를 전투의 함성과 피의 광란이 같이 쓰고 있어서,
	// 거기 노티파이를 달면 함성 이펙트가 피의 광란에서도 터진다.
	// 연출의 주인은 몽타주가 아니라 스킬 애셋이어야 한다.
	//
	// 쳐낼 조건: 한 스킬이 발동 중 여러 시점에 연출을 내야 하면 그건 타이밍이
	// 애니메이션에 묶인 것이다. 그때는 전용 몽타주를 파고 노티파이로 옮긴다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Cue")
	FGameplayTag ActivationCueTag;

	// 큐를 발밑에서 터뜨린다. 바닥에 퍼지는 연출은 true, 몸에서 나는 것은 false.
	// 액터 위치는 캡슐 중심이라 false 면 가슴 높이에서 난다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Cue")
	bool bActivationCueAtFeet = true;

	// ================ ActorInfo 를 직접 받는 것 ================
	// 위 오버라이드 넷에서 쓴다. CurrentActorInfo 가 아직 없는 시점이라 인자를 써야 한다.
	// GetASC / GetPlayerFrom 은 부모에 있다.

	// 증강의 마나 감소가 반영된 값. CheckCost 와 ApplyCost 가 같은 값을 봐야 한다.
	float GetFinalManaCostFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 증강의 쿨타임 감소가 반영된 값.
	float GetFinalCooldownFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 증강이 요구하는 추가 코스트 GE 를 붙인다. ApplyCost 가 부른다.
	void ApplyAdditionalCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const;

	// ================ 발동 중에만 쓰는 것 ================
	// GetPlayer / HasAugment / GetAugmentParam 은 부모에 있다.

	// 파생이 버프를 조기에 걷어야 할 때 부른다. 돌격의 공중 피해감소 등.
	void RemoveSelfBuff();

private:
	void ApplySelfBuff(const FGameplayAbilityActorInfo* ActorInfo);

	// ActivationCueTag 를 발동 위치에서 터뜨린다. 태그가 비어 있으면 아무것도 안 한다.
	void ExecuteActivationCue(const FGameplayAbilityActorInfo* ActorInfo);

	// DuringAbilityEffect 를 붙이고 뗀다. 어빌리티 수명과 정확히 같아야 하므로
	// 붙이는 곳과 떼는 곳을 한 쌍으로 묶어 두었다.
	void ApplyDuringAbilityEffect(const FGameplayAbilityActorInfo* ActorInfo);
	void RemoveDuringAbilityEffect(const FGameplayAbilityActorInfo* ActorInfo);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	// 이번 발동에서 붙인 버프 GE 핸들. 조기 제거용으로만 들고 있는다.
	TArray<FActiveGameplayEffectHandle> BuffHandles;

	// 발동 중 GE 핸들. 종료 시 떼려고 들고 있는다.
	FActiveGameplayEffectHandle DuringAbilityHandle;
};
