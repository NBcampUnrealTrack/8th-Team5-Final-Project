// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/GAS/EnemyMontageGABase.h"
#include "EnemyLeapGABase.generated.h"

/**
 * 타겟을 대상으로 도약 공격하는 GA
 * 원래는 시작/루프/끝을 해서 루프를 할수잇게하는 몽타주를 해보고 싶어서 Leap라 지었음
 * 하다보니 안될거 같아서 도약공격용이됨. 다른Leap용도는 맞춤형으로 제작필요.
 */
UCLASS()
class HEAVENBREACH_API UEnemyLeapGABase : public UEnemyMontageGABase
{
	GENERATED_BODY()
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;


protected:
	// 발사 노티파이를 받으면 포물선으로 던진다
	// 발사 타이밍은 애님노티파이 SendGameplay사용하자.
	UFUNCTION()
	void OnLaunchEventReceived(FGameplayEventData Payload);

	// 착지 시점은 몽타주 길이가 아니라 이동 시스템이 알려준다
	UFUNCTION()
	void OnCharacterLanded(const FHitResult& Hit);

	// 루프를 빠져나가 재생할 섹션 이름. 연속 도약이면 횟수만큼 넣는다.
	// 비워두면 섹션 점프를 안 하고 몽타주가 그대로 흘러간다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	TArray<FName> LandSectionNames = { TEXT("End") };

	// 포물선의 높이. 클수록 높이 뜨고 느리게 간다
	// SuggestProjectileVelocity_CustomArc함수에서 사용됨
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float ArcParam = 0.8f;

private:
	// 발사했는가를 저장해둔다.
	bool bLaunched = false;

	// 연속도약 기능이라면 현재 몇번째 러프인지 파악한다.
	int32 LeapIndex = 0;

};
