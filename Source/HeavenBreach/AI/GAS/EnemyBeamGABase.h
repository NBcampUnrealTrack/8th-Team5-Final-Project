// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/GAS/EnemyMontageGABase.h"
#include "EnemyBeamGABase.generated.h"

UENUM(BlueprintType)
enum class EHBBeamMode : uint8
{
	// 신호 1회당 판정 1회. 발사 순간 방향으로 고정된다.
	Instant       UMETA(DisplayName = "즉발"),

	// Fire 부터 Stop 까지 TickInterval 마다 판정. 매 틱 현재 방향이라 쓸고 간다.
	Continuous    UMETA(DisplayName = "지속")
};

UENUM(BlueprintType)
enum class EHBBeamAim : uint8
{
	// 머즐 소켓의 X축. VFX 와 같은 소켓을 보므로 연출과 판정이 원천적으로 일치한다.
	SocketForward  UMETA(DisplayName = "소켓 전방"),

	// 액터 전방. 몸통이 도는 대로 빔도 돈다.
	ActorForward   UMETA(DisplayName = "액터 전방"),

	// 현재 타겟을 직접 노린다. VFX 가 없거나 유도성 공격에만 쓴다.
	// 주의: VFX 는 애니메이션 방향으로 나가므로 이 모드는 연출과 어긋난다.
	Target         UMETA(DisplayName = "타겟 조준")
};

/**
 * 몽타주 재생에 더해 빔 판정을 하는 GA.
 *
 * VFX 는 몽타주 노티파이가 처리한다. 여기서는 판정만 한다.
 * 몽타주가 ASC 로 복제되므로 노티파이는 각 클라에서 알아서 울린다.
 */
UCLASS()
class HEAVENBREACH_API UEnemyBeamGABase : public UEnemyMontageGABase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 타이머를 반드시 여기서 끊는다. 안 그러면 어빌리티가 끝나거나 취소돼도 빔이 계속 나간다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 몽타주의 AN_SendGameplayEvent 가 보내는 신호
	UFUNCTION()
	void OnBeamFireEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnBeamStopEventReceived(FGameplayEventData Payload);

	// 판정 1회. 즉발은 한 번, 지속은 타이머가 반복해서 부른다.
	UFUNCTION()
	void FireBeamOnce();

	// ----- 모드 -----

	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	EHBBeamMode BeamMode = EHBBeamMode::Instant;

	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	EHBBeamAim AimMode = EHBBeamAim::SocketForward;

	// 지속 모드의 판정 주기.
	// 이 값이 곧 연타 속도라 대상별 재타격 쿨다운을 따로 두지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam", meta = (ClampMin = "0.05"))
	float TickInterval = 0.2f;

	// 이 어빌리티 1회당 최대 판정 수.
	//
	// 왜 GA 가 갖는가: 타수 제한이 VFX 에서 오는 제약이라 연출과 같은 층에 있어야 한다.
	// 몽타주 노티파이 개수에만 맡기면 잘못 깔았을 때 조용히 더 나간다.
	//
	// 쳐낼 조건: 타수를 몽타주로만 관리하기로 하면 이 값과 FiredCount 를 지운다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam", meta = (ClampMin = "1"))
	int32 MaxFireCount = 1;

	// ----- 판정 -----

	// 없으면 액터 위치에서 나간다. 눈에 띄게 어긋나므로 바로 알아챌 수 있다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	FName MuzzleSocketName = TEXT("Muzzle");

	// 소켓 X축이 빔 방향과 어긋날 때만 쓴다.
	// 소켓 회전을 직접 맞추는 게 먼저다. 그래야 VFX 와 구조적으로 같이 간다.
	//
	// 쳐낼 조건: 소켓 축을 제대로 맞추면 0 으로 두고 잊어도 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	FRotator MuzzleRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "Beam", meta = (ClampMin = "1.0"))
	float BeamRange = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Beam", meta = (ClampMin = "1.0"))
	float BeamRadius = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	float DamageMultiplier = 1.f;

	// false 면 첫 대상에서 멈춘다. 좁은 복도에서 관통은 꽤 세다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	bool bPenetrate = false;

	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel3;

	// ----- 디버깅 -----

	// 켜면 노란 트레이스 선이 그려진다. VFX 빔과 겹치는지 이걸로 확인한다.
	// 기본은 꺼 둔다. 플레이 중 화면을 가린다는 팀 요청. 확인할 때만 BP 에서 켠다.
	UPROPERTY(EditDefaultsOnly, Category = "Beam|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditDefaultsOnly, Category = "Beam|Debug")
	float DebugDrawTime = 1.f;

private:
	// 발사 지점과 방향을 구한다. 실패하면 false.
	bool ResolveBeamOrigin(FVector& OutStart, FVector& OutDirection) const;

	// 한 발이 대상에게 닿았을 때
	void ApplyBeamHit(AActor* Owner, AActor* Target, const FHitResult& Hit, float AttackPower);

	// 지속 모드의 타이머를 끊는다
	void StopBeamTick();

	// ActivateAbility 에서 0 으로 되돌린다
	int32 FiredCount = 0;

	FTimerHandle BeamTickTimer;
};
