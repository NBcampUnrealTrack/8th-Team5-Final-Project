// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_PlayerLeapAttack.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerLeapAttack : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_PlayerLeapAttack();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 발사 노티파이. 몽타주의 AN_SendGameplayEvent 가 Event.Leap.Launch 를 보낸다.
	UFUNCTION()
	void OnLaunchEventReceived(FGameplayEventData Payload);

	// 착지 시점은 몽타주 길이가 아니라 이동 시스템이 알려준다.
	UFUNCTION()
	void OnCharacterLanded(const FHitResult& Hit);

	// 착지를 못 한 채 시간이 지나면 강제로 마무리한다. 구멍에 빠지는 경우 대비.
	UFUNCTION()
	void OnAirTimeExpired();

	// ================ 애셋에서 지정 ================

	// 도약 거리. 시점 방향으로 이만큼 앞에 착지한다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float LeapDistance = 600.f;

	// 궤적의 둥근 정도. 0 에 가까울수록 수직, 1 에 가까울수록 낮고 길다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float ArcParam = 0.45f;

	// 도약 중에만 적용할 중력 배율. 클수록 빨리 뜨고 빨리 떨어진다.
	// 궤적 계산도 이 값으로 하므로 착지점은 변하지 않고 체공만 짧아진다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1"))
	float LeapGravityScale = 3.f;

	// 착지하면 점프할 몽타주 섹션. 비우면 섹션 전환 없이 몽타주가 그대로 흐른다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	FName LandSectionName = TEXT("Land");

	// 이 시간 안에 착지하지 못하면 강제로 끝낸다.
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1"))
	float MaxAirTime = 3.f;

	// 거리 배율은 증강 애셋의 Params 에서 SkillParam.Reach 로 온다.
	// 여기에 태그 칸을 두지 않는다. 스킬마다 태그 칸을 만들면 증강이 늘 때마다
	// 헤더를 고쳐야 하고, 그러면 애셋만으로 증강을 추가할 수 없다.

private:
	// 도약을 실제로 수행하는 머신인가. 서버와 소유 클라만 true.
	bool ShouldSimulateLeap() const;

	// 중력 배율을 원래대로 되돌린다. 착지·타임아웃·종료 세 곳에서 부른다.
	void RestoreGravity();

	// 착지 처리를 한 번만 하기 위한 플래그.
	bool bLaunched = false;

	// 중력을 우리가 바꿨는가. 안 바꿨는데 되돌리면 남의 값을 덮어쓴다.
	bool bGravityOverridden = false;

	// 발사 직전의 중력 배율. 다른 효과가 바꿔둔 값을 존중하려고 그때 저장한다.
	float SavedGravityScale = 1.f;

	FTimerHandle AirTimeTimer;
};
