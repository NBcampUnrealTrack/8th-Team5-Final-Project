// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/Magic/HBPlayerCastGABase.h"
#include "GA_MagicManaExplosion.generated.h"

/**
 * 마력 폭발 (마법 액티브, 설계 2.1). 1.6초 캐스팅 뒤 발동 순간 시전자 중심 반경 3m 의 적 전원에게 공격력 × 2.5 피해.
 * 증강: 위력(피해), 확장(반경).
 *
 * 피해는 발동 순간 이 어빌리티 안에서 즉시 준다(투사체 없음). 그래서 ModifyOutgoingDamage 를 기본값(현재 시전 인정)으로 부르고,
 * 잔류 마력용 NotifySkillHit 는 따로 부르지 않는다(현재 시전 인정이 그 역할을 한다).
 */
UCLASS()
class HEAVENBREACH_API UGA_MagicManaExplosion : public UHBPlayerCastGABase
{
	GENERATED_BODY()

public:
	UGA_MagicManaExplosion();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	float GetRadius() const { return Radius; }
	float GetDamageCoef() const { return DamageCoef; }

protected:
	virtual void OnCastReleased(const FGameplayAbilityActorInfo* Info) override;

	/** 기본 반경. 확장 증강(SkillParam.Radius)이 이 값을 대신한다 */
	UPROPERTY(EditDefaultsOnly, Category = "ManaExplosion", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 300.f;

	/** 공격력 배율. 위력 증강(SkillParam.Damage)이 곱해진다 */
	UPROPERTY(EditDefaultsOnly, Category = "ManaExplosion", meta = (ClampMin = "0.0"))
	float DamageCoef = 2.5f;
};
