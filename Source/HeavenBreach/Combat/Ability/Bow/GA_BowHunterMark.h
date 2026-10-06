// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_BowHunterMark.generated.h"

/**
 * 사냥꾼의 표식 (활 액티브, 설계 1장·2.3·2.12).
 * 발동 후 ApplyDelay 에 조준점(최대 AimTraceDistance) 반경 Radius 안의 살아 있는 적에게 표식 GE(UGE_HunterMark)를 MarkDuration 동안 붙인다.
 * 표식 대상이 받는 피해 증가는 AHBPlayerCharacter::ModifyOutgoingDamage 가 대상 태그로 계산한다(HBHunterMark).
 *
 * 비용·쿨다운·몽타주는 부모(UHBPlayerActiveGABase)가 시작하자마자 낸다. 표식은 서버에서만, 발동당 1회.
 * 몽타주가 없으면 부모가 바로 끝내므로, 그때는 직접 커밋하고 즉시 붙인 뒤 끝낸다.
 */
UCLASS()
class HEAVENBREACH_API UGA_BowHunterMark : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_BowHunterMark();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ================ 애셋에서 지정 ================
	// 몽타주는 부모의 Montage 를 쓴다.

	/** 발동부터 표식이 붙기까지. 공격속도를 따르지 않는다(bUseAttackSpeed=false) */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "0.0", Units = "s"))
	float ApplyDelay = 0.75f;

	/** 조준점 중심 구 반경. 넓은 시야 증강(SkillParam.Radius)이 최종값으로 덮는다 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 400.f;

	/** 표식 지속시간. GE 의 SetByCaller.Duration 으로 넘긴다 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "0.0", Units = "s"))
	float MarkDuration = 5.f;

	/** 조준점을 찾는 최대 거리. 아무것도 안 맞으면 이 거리의 허공이 중심이 된다 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 1800.f;

private:
	/** 서버 전용. 경직·사망 태그가 붙으면 표식 전에 취소로 끝낸다 */
	UFUNCTION()
	void OnInterruptTagAdded();

	/** 서버 전용. 발동당 1회 */
	void ApplyMarks();

	bool IsStaggeredOrDead() const;

	FTimerHandle ApplyTimerHandle;

	/** 이번 발동에서 표식을 이미 붙였는가. ActivateAbility 에서 초기화 */
	bool bMarkApplied = false;
};
