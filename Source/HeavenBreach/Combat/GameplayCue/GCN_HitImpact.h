// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayCueNotify_Burst.h"
#include "GCN_HitImpact.generated.h"

class USoundBase;

/**
 * 재질별 공격 적중 시 사운드/이펙트 Cue
 * 
 */
UCLASS()
class HEAVENBREACH_API UGCN_HitImpact : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	// 타격 성질별 소리. 키는 Impact.Slash / Impact.Blunt / Impact.Pierce 등.
	// 채우지 않은 성질은 DefaultSound를 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TMap<FGameplayTag, TObjectPtr<USoundBase>> ImpactSounds;

	// 위 표에 없는 성질이거나, 공격자가 성질 태그를 안 갖고 있을 때.
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TObjectPtr<USoundBase> DefaultSound;
};
