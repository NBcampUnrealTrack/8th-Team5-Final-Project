// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/Magic/HBPlayerCastGABase.h"
#include "GA_MagicFrostField.generated.h"

class AHBFrostField;

/**
 * 서리 지대 (마법 액티브, 설계 2.1). 0.35초 뒤 조준점(최대 12m) 바닥에 반경 3m 지대를 4초간 깐다.
 * 지대 안 적은 이동 속도 -40%. 증강: 혹한(감속), 확장(반경).
 *
 * 지점은 시작 때 고정한다(OnCastStarted). 시전 중 시선을 돌려도 처음 겨눈 곳에 깔린다.
 * 조준점 아래에 지면이 없으면 CanRelease 가 false 라 비용 없이 끝난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_MagicFrostField : public UHBPlayerCastGABase
{
	GENERATED_BODY()

public:
	UGA_MagicFrostField();

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	float GetRadius() const { return Radius; }
	float GetDuration() const { return Duration; }
	float GetSlow() const { return Slow; }
	float GetAimTraceDistance() const { return AimTraceDistance; }

protected:
	virtual void OnCastStarted(const FGameplayAbilityActorInfo* Info) override;
	virtual bool CanRelease(const FGameplayAbilityActorInfo* Info) override;
	virtual void OnCastReleased(const FGameplayAbilityActorInfo* Info) override;

	/** 지대 클래스. BP 에서 바꿀 수 있다 */
	UPROPERTY(EditDefaultsOnly, Category = "FrostField")
	TSubclassOf<AHBFrostField> FieldClass;

	/** 기본 반경. 확장 증강(SkillParam.Radius)이 이 값을 대신한다 */
	UPROPERTY(EditDefaultsOnly, Category = "FrostField", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 300.f;

	/** 지속 시간. 0 이면 액터 수명이 무한이 되므로 0.1초 이상(AHBFrostField::Init 도 막는다) */
	UPROPERTY(EditDefaultsOnly, Category = "FrostField", meta = (ClampMin = "0.1", Units = "s"))
	float Duration = 4.f;

	/** 기본 이동 속도 감소 비율. 혹한 증강(SkillParam.Slow)이 이 값을 대신한다 */
	UPROPERTY(EditDefaultsOnly, Category = "FrostField", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Slow = 0.4f;

	/** 조준점을 찾는 최대 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "FrostField", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 1200.f;

private:
	/** 시작 때 고정한 지면 지점. bTargetValid 가 true 일 때만 유효 */
	FVector TargetPoint = FVector::ZeroVector;
	bool bTargetValid = false;
};
