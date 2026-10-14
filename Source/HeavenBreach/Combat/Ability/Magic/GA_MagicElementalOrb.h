// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/Magic/HBPlayerCastGABase.h"
#include "GA_MagicElementalOrb.generated.h"

class AHBMageOrb;

/**
 * 원소 구체 (마법 액티브, 설계 2.1). 캐스팅 0.4초 뒤 왼손(hand_l)에서 크로스헤어 방향으로 구체를 쏜다.
 * 구체는 처음 맞은 적 1명에게 공격력 × 1.6 피해. 증강: 위력(피해), 속사(쿨다운).
 * 공통 시전 흐름은 UHBPlayerCastGABase. 이 클래스는 발동 시 구체를 서버에서 스폰하는 일만 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_MagicElementalOrb : public UHBPlayerCastGABase
{
	GENERATED_BODY()

public:
	UGA_MagicElementalOrb();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	float GetOrbSpeed() const { return Speed; }
	float GetDamageCoef() const { return DamageCoef; }
	float GetExplosionRadius() const { return ExplosionRadius; }
	float GetEdgeDamageFraction() const { return EdgeDamageFraction; }

protected:
	virtual void OnCastReleased(const FGameplayAbilityActorInfo* Info) override;

	/** 구체 클래스. BP 에서 꼬리 이펙트 등을 얹은 자식으로 바꿀 수 있다 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb")
	TSubclassOf<AHBMageOrb> OrbClass;

	/** 구체 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Speed = 1800.f;

	/** 공격력 배율. 위력 증강(SkillParam.Damage)이 곱해진다 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.6f;

	/** 맞은 지점의 폭발 반경. 폭발 범위 증강(SkillParam.Radius)이 이 값을 대신한다 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 200.f;

	/** 폭발 가장자리의 피해 비율. 중심 100% 에서 이 값까지 거리에 따라 줄어든다 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeDamageFraction = 0.5f;

	/** 조준점을 찾는 최대 거리. 아무것도 안 맞으면 이 거리의 허공을 향해 쏜다 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 10000.f;

	/** 시작점으로 쓰는 몸 메시 소켓 */
	UPROPERTY(EditDefaultsOnly, Category = "ElementalOrb")
	FName MuzzleSocketName = TEXT("hand_l");
};
