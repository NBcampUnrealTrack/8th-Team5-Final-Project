// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/Actor.h"
#include "HBFrostField.generated.h"

class AEnemyCharacterBase;
class AHBPlayerCharacter;

/**
 * 서리 지대 (마법스킬 설계 2.1). 바닥에 깔려 Duration 동안 반경 안 적에게 이동 속도 감속(UGE_FrostSlow)을 건다.
 * 생기는 순간과 이후 DamageInterval 마다 안의 살아 있는 적에게 도트 피해(공격력 × DamageCoef × 마법 피해 배율)를 준다.
 *
 * 감속·판정은 서버만 한다. 0.25초마다 구 오버랩으로 영역 안 살아 있는 적을 찾아, 아직 감속이 없으면 붙이고
 * 영역을 벗어났거나 죽은 적은 뗀다. 같은 지대가 한 적에게 감속을 두 번 걸지 않는다. 지대가 사라질 때(EndPlay) 남은 감속은 모두 뗀다.
 *
 * 연출은 클라(데디케이트 서버 제외)가 BeginPlay 에서 로컬로 스폰한다(복제하지 않는다).
 * Radius·Duration 은 복제한다. 서버가 SpawnActorDeferred 로 Init 한 뒤 FinishSpawning 하므로 첫 복제 번들에 값이 실려
 * 클라 BeginPlay 에서 이미 올바른 값을 읽는다.
 */
UCLASS()
class HEAVENBREACH_API AHBFrostField : public AActor
{
	GENERATED_BODY()

public:
	AHBFrostField();

	/** 서버. SpawnActorDeferred 직후, FinishSpawning 전에 부른다. 수명을 Duration 으로 정한다 */
	void Init(AHBPlayerCharacter* InOwner, float InRadius, float InDuration, float InSlow, float InDamageCoef, float InDamageInterval, int32 InCastId);

	float GetRadius() const { return Radius; }
	float GetDuration() const { return Duration; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** 서버 전용. 0.25초마다 */
	void TickField();

	/** 서버 전용. DamageInterval 마다 영역 안 적에게 도트 피해 */
	void TickDamage();

	/** 서버 전용. 감속을 건 모든 적에서 뗀다 */
	void RemoveAllSlows();

	/** 서버 전용. 한 적의 감속을 뗀다 */
	void RemoveSlow(const FActiveGameplayEffectHandle& Handle);

	/** 클라(데디 제외). 지대 연출 BP 를 로컬로 스폰한다 */
	void SpawnLocalVisual();

	/** 영역 판정 주기 */
	static constexpr float TickInterval = 0.25f;

	UPROPERTY(Replicated)
	float Radius = 300.f;

	UPROPERTY(Replicated)
	float Duration = 4.f;

	/** 서버 전용. 감속량(양수). 적용할 때 음수로 넘긴다 */
	float Slow = 0.4f;

	/** 서버 전용. 도트 피해 계수·간격, 잔류 마력용 시전 ID */
	float DamageCoef = 0.f;
	float DamageInterval = 1.f;
	int32 CastId = 0;
	FTimerHandle DamageTimerHandle;

	TWeakObjectPtr<AHBPlayerCharacter> Caster;

	/** 서버 전용. 적별 감속 핸들 */
	TMap<TWeakObjectPtr<AEnemyCharacterBase>, FActiveGameplayEffectHandle> SlowHandles;

	FTimerHandle TickTimerHandle;
};
