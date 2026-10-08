// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBMageOrb.generated.h"

class AHBPlayerCharacter;
class UNiagaraComponent;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * 원소 구체 투사체 (마법스킬 설계 2.1).
 * 처음 맞은 대상 하나에 피해를 주고 사라진다. 벽·바닥에 맞아도 사라진다. 맞히지 못하면 InitialLifeSpan(4초) 뒤 사라진다.
 *
 * 충돌: 화살과 같은 BlockAllDynamic(Visibility·Camera 무시), 이벤트는 OnComponentHit.
 * "Projectile" 프로필은 쓰지 않는다 — 그 프로필은 "Enemy" 채널을 무시하고, Pawn 프로필도 Projectile 오브젝트를 무시해 적에게 아예 안 맞는다.
 * 피해·큐는 서버에서만. 액터는 복제되고 이동도 복제된다.
 */
UCLASS()
class HEAVENBREACH_API AHBMageOrb : public AActor
{
	GENERATED_BODY()

public:
	AHBMageOrb();

	/** 서버. 스폰 직후 부른다. 발사 속도·피해 계수·시전 ID 를 정하고 날린다 */
	void Launch(AHBPlayerCharacter* InShooter, const FVector& Dir, float InSpeed, float InDamageCoef, int32 InCastId);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	/** 서버 전용. 맞은 대상이 피해를 받을 수 있으면 피해를 준다 */
	void ApplyOrbDamage(AActor* Target, const FHitResult& Hit);

	/** 서버 전용. 시전자 ASC 로 적중 큐를 실행한다 */
	void PlayImpactCue(const FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	/** 꼬리 이펙트. 에셋은 BP 에서 지정(기본 비어 있음) */
	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UNiagaraComponent> TrailComp;

	TWeakObjectPtr<AHBPlayerCharacter> Shooter;
	float DamageCoef = 1.f;
	int32 CastId = 0;

	/** 한 발에 한 번만 처리한다 */
	bool bHasHit = false;
};
