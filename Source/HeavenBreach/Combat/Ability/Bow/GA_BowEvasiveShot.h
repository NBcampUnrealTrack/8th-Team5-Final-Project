// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Engine/DataTable.h"
#include "GA_BowEvasiveShot.generated.h"

class AHBPlayerArrow;

/**
 * 회피 공격 (활 액티브, 설계 1장·2.9·2.10).
 * 뒤로 RetreatDistance 만큼 물러나며(RetreatStart~RetreatStart+RetreatDuration) FireDelay 에 화살 1발을 자동으로 쏜다. 공중 사용 불가.
 *
 * 비용·쿨다운·몽타주는 부모(UHBPlayerActiveGABase)가 시작하자마자 낸다. 화살이 없으면 커밋 전에 거절(무소모 + 안내).
 * 후퇴는 UAbilityTask_ApplyRootMotionConstantForce 로 서버·예측 클라가 같이 준다(CMC 루트 모션 예측).
 * 발사는 서버에서만, 발동당 1회. 몽타주가 없으면 부모가 바로 끝내므로 직접 커밋하고 FireDelay+0.1 뒤 끝낸다.
 */
UCLASS()
class HEAVENBREACH_API UGA_BowEvasiveShot : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_BowEvasiveShot();

	// 공중이면 막는다. 화살은 여기서 보지 않는다 — 여기서 막으면 "화살이 없습니다" 를 띄울 곳이 없다(ActivateAbility 에서 본다).
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 테스트·UI 용. CDO 에서 불러도 된다.
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ================ 애셋에서 지정 ================
	// 몽타주는 부모의 Montage 를 쓴다.

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot")
	TSubclassOf<AHBPlayerArrow> ArrowClass;

	/** 발사할 때 가방에서 1개 쓰는 화살 아이템. 비어 있으면 무한 */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Ammo")
	FDataTableRowHandle ArrowItemRow;

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Ammo")
	FText NoArrowMessage = NSLOCTEXT("BowEvasiveShot", "NoArrow", "화살이 없습니다");

	/** 뒤로 물러나는 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Retreat", meta = (ClampMin = "0.0", Units = "cm"))
	float RetreatDistance = 97.5f;

	/** 발동부터 후퇴 시작까지 */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Retreat", meta = (ClampMin = "0.0", Units = "s"))
	float RetreatStart = 0.067f;

	/** 후퇴에 걸리는 시간. 속도 = RetreatDistance / RetreatDuration */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Retreat", meta = (ClampMin = "0.01", Units = "s"))
	float RetreatDuration = 0.433f;

	/** true 면 GA 가 후퇴를 주지 않고 몽타주 루트 모션에 맡긴다(정식 AnimBP 이관 뒤) */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot|Retreat")
	bool bUseMontageRootMotion = false;

	/** 발동부터 화살이 나가기까지(몽타주의 발사 프레임) */
	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot", meta = (ClampMin = "0.0", Units = "s"))
	float FireDelay = 0.767f;

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ArrowSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 10000.f;

	UPROPERTY(EditDefaultsOnly, Category = "EvasiveShot", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnForwardOffset = 50.f;

private:
	/** 서버·예측 클라. RetreatStart 뒤 루트 모션 후퇴 */
	UFUNCTION()
	void StartRetreat();

	/** 서버 전용. 경직·사망 태그가 붙으면 발사 전에 취소로 끝낸다 */
	UFUNCTION()
	void OnInterruptTagAdded();

	/** 서버 전용. 발동당 1회 */
	void FireArrow();

	/** 몽타주 없는 경로의 끝 */
	void FinishWithoutMontage();

	bool IsStaggeredOrDead() const;

	/** 이 스킬의 화살 클래스·행. 비었으면 활 평타 GA 값으로 채운다(HBBowShot::ResolveArrowDefaults) */
	void ResolveArrow(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& OutClass, FDataTableRowHandle& OutRow) const;

	FTimerHandle FireTimerHandle;
	FTimerHandle FinishTimerHandle;

	/** 이번 발동에서 화살을 이미 쐈는가(시도 포함). ActivateAbility 에서 초기화 */
	bool bArrowFired = false;
};
