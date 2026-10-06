// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Engine/DataTable.h"
#include "GA_BowPiercingShot.generated.h"

class AHBPlayerArrow;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * 관통 사격 (활 액티브, 설계 1장).
 * Q/E → 준비 → 좌클릭을 누르고 있으면 당김 → DrawTime 이상 당긴 뒤 놓으면 관통 화살 발사.
 *
 * 비용·쿨다운·화살은 발사 순간 서버에서만 낸다(지연 커밋). 그래서 부모의 ActivateAbility 를 부르지 않는다.
 * 부모는 시작하자마자 CommitAbility 를 하기 때문이다. 부모의 자버프·발동 중 GE·발동 큐도 쓰지 않는다.
 * 다 당기기 전에 놓기, 우클릭(캐릭터가 CancelAbilityHandle) = 취소, 아무것도 내지 않는다.
 *
 * 좌클릭은 State.Bow.SkillAiming 이 붙어 있는 동안 캐릭터가 이 스킬 스펙으로 보낸다(WantsAimInput).
 * 발사 여부는 서버가 잰 당김 시간으로만 정한다. 클라는 자기가 잰 시간으로 몽타주만 고른다.
 */
UCLASS()
class HEAVENBREACH_API UGA_BowPiercingShot : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_BowPiercingShot();

	virtual bool WantsAimInput() const override { return true; }

	// 테스트·UI 용. CDO 에서 불러도 된다.
	int32 GetMaxPierceCount() const { return MaxPierceCount; }
	const FGameplayTagContainer& GetRequiredWeaponTags() const { return RequiredWeaponTags; }
	const FGameplayTagContainer& GetActivationOwnedTags() const { return ActivationOwnedTags; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 우클릭 취소 중에는 몽타주 태스크의 OnCancelled 가 다시 끝내려 들지 않게 막는다.
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

	// ================ 애셋에서 지정 ================

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot")
	TSubclassOf<AHBPlayerArrow> ArrowClass;

	/** 발사할 때 가방에서 1개 쓰는 화살 아이템. 비어 있으면 무한 */
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Ammo")
	FDataTableRowHandle ArrowItemRow;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Ammo")
	FText NoArrowMessage = NSLOCTEXT("BowPiercingShot", "NoArrow", "화살이 없습니다");

	// 몽타주는 모두 비어 있어도 로직이 돈다(설계 2.11). 준비·당김은 마지막 포즈에서 멈추게(auto blend-out 끔) 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Montage")
	TObjectPtr<UAnimMontage> PrepareMontage;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Montage")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Montage")
	TObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot|Montage")
	TObjectPtr<UAnimMontage> CancelMontage;

	/** 이만큼 당긴 뒤 놓아야 발사. 그 전에 놓으면 취소 */
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0", Units = "s"))
	float DrawTime = 0.833f;

	/** 놓은 뒤 화살이 나가기까지(Release 몽타주의 발사 프레임) */
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0", Units = "s"))
	float FireDelay = 0.083f;

	/** 발사·취소 몽타주가 없을 때 끝나기까지 기다리는 시간. FireDelay 보다 짧으면 FireDelay 를 쓴다 */
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0", Units = "s"))
	float FinishRecoveryTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0"))
	float DamageCoef = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ArrowSpeed = 4000.f;

	/**
	 * 뚫고 지나가며 피해를 주는 Pawn 수. 그 다음 Pawn 에는 박히며 피해를 준다.
	 * 4 면 4명 관통 + 5번째에 박힘 = 설계의 "최대 5명".
	 */
	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0"))
	int32 MaxPierceCount = 4;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 10000.f;

	UPROPERTY(EditDefaultsOnly, Category = "PiercingShot", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnForwardOffset = 50.f;

private:
	UFUNCTION()
	void OnInputPressed(float TimeWaited);

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnFinishMontageEnded();

	UFUNCTION()
	void OnFinishMontageCancelled();

	/** 서버 전용. 경직·사망 태그가 붙으면 취소 경로로 */
	UFUNCTION()
	void OnInterruptTagAdded();

	/** 준비·당김 몽타주가 다른 몽타주(경직 등)에 끊기면 취소 경로로 */
	UFUNCTION()
	void OnPhaseMontageInterrupted();

	/** 발사 전 끊김(경직·사망·몽타주 끊김). 취소 몽타주 없이 바로 끝낸다. 비용·쿨다운·화살 없음 */
	void CancelByInterrupt(const TCHAR* Reason);

	/** 경직·사망 중인가. 서버의 발사 직전 방어 검사 */
	bool IsStaggeredOrDead() const;

	void EndPhaseMontageTask(TWeakObjectPtr<UAbilityTask_PlayMontageAndWait>& Task);

	/** 준비가 끝났다. 이미 눌려 있으면 바로 당긴다 */
	void OnPrepareFinished();

	void BeginDraw();

	/** 서버 전용. 비용·쿨다운 커밋 + 화살 소모 + FireDelay 타이머. 실패하면 false(취소 경로로) */
	bool TryCommitFire();

	/** 서버 전용. 발동당 1회. 사망 중이면 쏘지 않는다 */
	void SpawnPiercingArrow();

	/** 이 스킬의 화살 클래스·행. 비었으면 활 평타 GA 값으로 채운다(HBBowShot::ResolveArrowDefaults) */
	void ResolveArrow(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& OutClass, FDataTableRowHandle& OutRow) const;

	/** Release·Cancel 몽타주 뒤 끝낸다. 몽타주가 없으면 FinishRecoveryTime 뒤 */
	void PlayFinishAndEnd(UAnimMontage* FinishMontage, bool bCancelled);

	void FinishSelf(bool bCancelled);

	void SetDrawingPose(bool bDrawing) const;

	/** 다음 단계 몽타주가 이전 것을 끊을 때 OnInterrupted 가 취소로 오인되지 않게 먼저 끝낸다 */
	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> PrepareTask;
	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> DrawTask;

	FTimerHandle PrepareTimerHandle;
	FTimerHandle FireTimerHandle;
	FTimerHandle FinishTimerHandle;

	/** 이번 발동의 진행 상태. ActivateAbility 에서 모두 초기화한다 */
	bool bPrepareDone = false;
	bool bInputPressed = false;
	bool bInputReleased = false;
	bool bDrawStarted = false;
	bool bFinishing = false;
	bool bFireCommitted = false;
	bool bArrowSpawned = false;
	bool bFinishCancelled = false;
	bool bCancelling = false;
	float DrawStartTime = 0.f;
};
