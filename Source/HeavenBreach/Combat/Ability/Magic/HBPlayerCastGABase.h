// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "HBPlayerCastGABase.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * 마법 액티브 공통(마법스킬 설계 2.2-1). 시전 → 발동 → 마무리.
 *
 * 비용·쿨다운은 발동 순간 서버에서만 낸다(지연 커밋, 관통 사격과 같다). 그래서 부모의 ActivateAbility 를 부르지 않는다.
 * 부모는 시작하자마자 CommitAbility 를 하기 때문이다. 부모의 자버프·발동 중 GE·발동 큐도 쓰지 않는다.
 *
 * 흐름
 *   시작   CheckCost 만 → 몽타주(재생 속도 = MontagePlayRate / 캐스팅 배율) → 손 준비 큐(서버) →
 *          bIsCasting 이면 감속 GE(서버) + State.Casting(우클릭 취소용) → OnCastStarted(서버)
 *   발동   서버 타이머 ReleaseTime / MontagePlayRate × 캐스팅 배율. CanRelease false 면 무소모 종료,
 *          true 면 CommitAbility → OnCastReleased → 감속·태그·손 큐 제거
 *   마무리 몽타주가 끝나면(없으면 FinishRecoveryTime 뒤) 서버가 끝낸다. 클라는 서버 종료 복제로 끝난다.
 *   취소   발동 전 경직·사망(서버 태그 대기)·우클릭(캐릭터 TryParry)·몽타주 끊김. 비용·효과 없음.
 *
 * 몽타주가 비어 있어도 로직이 돈다. 몽타주·수치는 BP 자식이나 C++ 자식이 정한다.
 */
UCLASS(Abstract)
class HEAVENBREACH_API UHBPlayerCastGABase : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UHBPlayerCastGABase();

	/** 캐스팅 스킬만 집중 패시브의 배율을 받는다. 아니면 1. 인스턴스 전용(아바타를 본다) */
	float GetCastMultiplier() const;

	// 테스트·UI 용. CDO 에서 불러도 된다.
	float GetReleaseTime() const { return ReleaseTime; }
	bool IsCastingSkill() const { return bIsCasting; }
	float GetMontagePlayRate() const { return MontagePlayRate; }
	float GetFinishRecoveryTime() const { return FinishRecoveryTime; }
	FVector GetCastHandOffset() const { return CastHandOffset; }
	float GetCastHandScale() const { return CastHandScale; }
	int32 GetCastHandFxIndex() const { return CastHandFxIndex; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 취소 중에는 몽타주 태스크의 OnCancelled 가 다시 끝내려 들지 않게 막는다(관통 사격과 같다).
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

	// ================ 자식이 구현 ================

	/** 서버. 시전 준비(몽타주·큐·감속)를 마친 직후 1회. 서리 지대가 여기서 지점을 고정한다 */
	virtual void OnCastStarted(const FGameplayAbilityActorInfo* Info) {}

	/** 서버. 발동 순간 커밋 전에 1회. false 면 비용 없이 끝낸다(취소 아님). 순간 이동의 목적지 검사 */
	virtual bool CanRelease(const FGameplayAbilityActorInfo* Info) { return true; }

	/** 서버. 커밋(비용·쿨다운) 성공 직후 1회. 스킬 고유 동작 */
	virtual void OnCastReleased(const FGameplayAbilityActorInfo* Info) {}

	// ================ 애셋에서 지정 ================

	/** 발동 시각(s). 몽타주를 재생 속도 1 로 틀었을 때 기준. 실제 시간은 MontagePlayRate·캐스팅 배율로 나눈다 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast", meta = (ClampMin = "0.0", Units = "s"))
	float ReleaseTime = 0.f;

	/** 캐스팅 스킬인가. true 면 집중 배율·이동 감속(UGE_CastingSlow)·State.Casting(우클릭 취소)이 붙는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	bool bIsCasting = false;

	/** 몽타주 재생 속도. 순간 이동은 2.0 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	/** 발동 뒤 몽타주가 없거나 이미 끝났을 때 끝나기까지 기다리는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast", meta = (ClampMin = "0.0", Units = "s"))
	float FinishRecoveryTime = 0.3f;

	/** 손 준비 큐의 hand_l 기준 로컬 오프셋. 큐 파라미터 Location 으로 넘긴다 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast|Cue")
	FVector CastHandOffset = FVector(18.f, -6.f, 0.f);

	/** 손 준비 이펙트 크기. 큐 파라미터 RawMagnitude 로 넘긴다 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast|Cue", meta = (ClampMin = "0.0"))
	float CastHandScale = 0.2f;

	/** 손 준비 이펙트 종류. 큐 파라미터 AbilityLevel 로 넘긴다. 1 구체, 2 순간 이동, 3 서리, 4 폭발 */
	UPROPERTY(EditDefaultsOnly, Category = "Cast|Cue", meta = (ClampMin = "0"))
	int32 CastHandFxIndex = 0;

private:
	/** 발동 타이머. 서버는 발동 처리, 클라는 시전 단계만 넘긴다(State.Casting 제거) */
	void OnReleaseTime();

	/** 서버 전용. CanRelease → CommitAbility → OnCastReleased */
	void ReleaseCast();

	UFUNCTION()
	void OnCastMontageEnded();

	UFUNCTION()
	void OnCastMontageInterrupted();

	/** 서버 전용. 경직·사망 태그가 붙으면 발동 전이면 취소 */
	UFUNCTION()
	void OnInterruptTagAdded();

	/** 발동 전 끊김. 비용·효과 없이 CancelAbility */
	void CancelByInterrupt(const TCHAR* Reason);

	bool IsStaggeredOrDead() const;

	/** 서버 전용. 정상 종료(복제) */
	void FinishSelf(bool bCancelled);

	/** 발동 뒤 몽타주가 없거나 끝났으면 FinishRecoveryTime 뒤 끝낸다 */
	void ScheduleFinish();

	void AddHandCue(const FGameplayAbilityActorInfo* Info);

	/** 감속 GE·State.Casting·손 큐를 뗀다. 여러 번 불려도 된다 */
	void RemoveCastingState(const FGameplayAbilityActorInfo* Info);

	/** 이번 발동에서 재생한 몽타주. 취소되면 멈춘다 */
	UPROPERTY()
	TObjectPtr<UAnimMontage> PlayingMontage;

	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FActiveGameplayEffectHandle CastingSlowHandle;

	FTimerHandle ReleaseTimerHandle;
	FTimerHandle FinishTimerHandle;

	/** 이번 발동의 진행 상태. ActivateAbility 에서 모두 초기화한다 */
	bool bReleased = false;
	bool bMontageEnded = false;
	bool bFinishing = false;
	bool bCancelling = false;
	bool bCastingTagAdded = false;
	bool bHandCueAdded = false;
};
