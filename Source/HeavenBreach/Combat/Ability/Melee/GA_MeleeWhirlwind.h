// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_MeleeWhirlwind.generated.h"

/**
 * 회전 베기 (근접 액티브, 설계 1장). 발동 후 HitDelay 에 시전자 중심 반경 Radius 안의 살아 있는 적 전원에게 공격력 × DamageCoef 피해.
 * 증강: 위력(피해 배율), 확장(반경).
 *
 * 비용·쿨다운·몽타주는 부모(UHBPlayerActiveGABase)가 시작하자마자 낸다. 피해는 서버에서만, 발동당 1회.
 * 즉시 스킬 피해라 ModifyOutgoingDamage 를 기본값(현재 시전 인정)으로 부른다.
 * 몽타주가 없으면 부모가 바로 끝내므로, 그때는 직접 커밋하고 즉시 판정한 뒤 끝낸다.
 */
UCLASS()
class HEAVENBREACH_API UGA_MeleeWhirlwind : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_MeleeWhirlwind();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	float GetHitDelay() const { return HitDelay; }
	float GetRadius() const { return Radius; }
	float GetDamageCoef() const { return DamageCoef; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ================ 애셋에서 지정 ================
	// 몽타주는 부모의 Montage 를 쓴다.

	/** 발동부터 판정까지. 공격속도를 따르지 않는다(bUseAttackSpeed=false) */
	UPROPERTY(EditDefaultsOnly, Category = "Whirlwind", meta = (ClampMin = "0.0", Units = "s"))
	float HitDelay = 0.4f;

	/** 시전자 중심 구 반경. 확장 증강(SkillParam.Radius)이 최종값으로 덮는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Whirlwind", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 250.f;

	/** 공격력 배율. 위력 증강(SkillParam.Damage)이 곱해진다 */
	UPROPERTY(EditDefaultsOnly, Category = "Whirlwind", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.8f;

private:
	/** 서버 전용. 경직·사망 태그가 붙으면 판정 전에 취소로 끝낸다 */
	UFUNCTION()
	void OnInterruptTagAdded();

	/** 서버 전용. 발동당 1회 */
	void ApplyHits();

	bool IsStaggeredOrDead() const;

	FTimerHandle HitTimerHandle;

	/** 이번 발동에서 판정을 이미 했는가. ActivateAbility 에서 초기화 */
	bool bHitApplied = false;
};
