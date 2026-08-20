// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/GAS/EnemyMontageGABase.h"
#include "EnemyProjGABase.generated.h"

class AEnemyProjectile;

/**
 * 기존 부모의 GA의 몽타주 재생에 더해 발사준비와 발사를 하는 GA
 */
UCLASS()
class HEAVENBREACH_API UEnemyProjGABase : public UEnemyMontageGABase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 발사 신호를 받으면 투사체를 하나 스폰한다.
	UFUNCTION()
	void OnFireEventReceived(FGameplayEventData Payload);

	// 스폰할 투사체
	// 투사체 액터는 커스텀으로 만든거.
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AEnemyProjectile> Projectile;

	// 발사 지점 소켓. 없으면 액터 위치를 기준으로 한다.
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	FName MuzzleSocketName = TEXT("hand_r");

	// 대상 중심이 아닌 그것에서 좀더 보정
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float AimHeightOffset = 20.f;
};
