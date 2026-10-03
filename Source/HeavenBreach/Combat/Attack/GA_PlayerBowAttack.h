// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataTable.h"
#include "GA_PlayerBowAttack.generated.h"

class AHBPlayerArrow;
class AHBPlayerCharacter;

/**
 * 활 기본공격. 누르고 있으면 당기고, 떼면 당긴 시간만큼 센 화살을 쏜다.
 * 어느 무기용인지는 BP 의 AssetTags 에 Weapon.Bow 를 넣어 정한다(검 평타 BP 와 같은 방식).
 * 떼기는 AHBPlayerCharacter::StopAttack 이 InputReleased 이벤트로 알려 준다.
 * 당긴 시간은 서버가 잰 값을 쓴다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerBowAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerBowAttack();

	static UGA_PlayerBowAttack* FindActive(UAbilitySystemComponent* ASC);

	/** 가방에서 쓰는 화살 아이템 행. 비어 있으면 화살을 쓰지 않는다(무한) */
	const FDataTableRowHandle& GetArrowItemRow() const { return ArrowItemRow; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnReleased(float TimeHeld);

	UFUNCTION()
	void OnFinishMontageEnded();

	UFUNCTION()
	void OnFinishMontageCancelled();

	UPROPERTY(EditDefaultsOnly, Category = "Bow")
	TSubclassOf<AHBPlayerArrow> ArrowClass;

	/** 쏠 때마다 가방에서 1개씩 쓰는 화살 아이템(DT_Item / Arrow). 비어 있으면 무한 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow|Ammo")
	FDataTableRowHandle ArrowItemRow;

	/** 화살이 없어서 당기지 못할 때 화면에 띄우는 문구 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow|Ammo")
	FText NoArrowMessage = NSLOCTEXT("PlayerBow", "NoArrow", "화살이 없습니다");

	/**
	 * 당기기. 마지막 포즈에서 멈추게(auto blend-out 끔) 만든다. 비어 있어도 동작한다.
	 * 무기 외형 표(DT_WeaponPresentation)에 몽타주가 있으면 그쪽이 우선이다. 여기 값은 폴백.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow|Montage")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Montage")
	TObjectPtr<UAnimMontage> ReleaseMontage;

	/** 최소 당김 전에 뗐을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow|Montage")
	TObjectPtr<UAnimMontage> CancelMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0", Units = "s"))
	float MinDrawTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0", Units = "s"))
	float MaxDrawTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinArrowSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxArrowSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MinDamageCoef = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MaxDamageCoef = 1.0f;

	/** 발사·취소 몽타주가 없을 때 끝나기까지 기다리는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0", Units = "s"))
	float ReleaseRecoveryTime = 0.3f;

	/** 카메라에서 이만큼 앞에 화살을 만든다. 내 캡슐과 겹치지 않게 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnForwardOffset = 50.f;

	/** 조준점을 찾는 트레이스 거리. 아무것도 안 맞으면 이 거리의 점을 향한다 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 10000.f;

	/** 당기는 동안 이동 감속 GE. 검 평타와 같은 것을 꽂는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Bow")
	TSubclassOf<UGameplayEffect> AttackSlowEffect;

private:
	/** 가방에 화살이 1개 이상 있는지. 행이 비어 있으면 항상 true */
	bool HasArrow(const AHBPlayerCharacter* Player) const;

	/** 서버 전용. 화살 1개를 쓴다. 행이 비어 있으면 아무것도 안 쓰고 true */
	bool ConsumeArrow(AHBPlayerCharacter* Player) const;

	/** 서버 전용. 시선 방향으로 화살을 만든다 */
	void SpawnArrow(AHBPlayerCharacter* Player, float Speed, float DamageCoef);

	/** 발사·취소 뒤 마무리. 몽타주가 있으면 끝날 때, 없으면 ReleaseRecoveryTime 뒤에 끝낸다 */
	void PlayFinishAndEnd(UAnimMontage* Montage);

	void EndSelf(bool bWasCancelled);

	void RemoveAttackSlow(const FGameplayAbilityActorInfo* ActorInfo);

	/** 외형 표의 몽타주가 있으면 그것, 없으면 이 GA 의 값 */
	UAnimMontage* ResolveMontage(UAnimMontage* FromPresentation, UAnimMontage* Fallback) const;

	/** 이번 발동에 쓰는 몽타주. 발동할 때 외형 표에서 정한다 */
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveDrawMontage;

	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveReleaseMontage;

	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveCancelMontage;

	FActiveGameplayEffectHandle SlowHandle;

	FTimerHandle RecoveryTimerHandle;
};
