// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HBMageOrb.generated.h"

class AHBPlayerCharacter;
class UNiagaraComponent;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * 원소 구체 투사체 (마법스킬 설계 2.1).
 * 적·벽·바닥 어디든 처음 막히는 곳에서 폭발한다(반경 ExplosionRadius, 중심 100% → 가장자리 EdgeFraction).
 * 직접 맞은 적은 100%, 나머지 적은 거리에 따라. 한 적은 한 번만 맞는다. 맞히지 못하면 InitialLifeSpan(4초) 뒤 터지지 않고 사라진다.
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
	void Launch(AHBPlayerCharacter* InShooter, const FVector& Dir, float InSpeed, float InDamageCoef, int32 InCastId, float InExplosionRadius, float InEdgeFraction);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	/** 서버 전용. Center 에서 폭발해 반경 안의 살아 있는 적에게 피해를 준다. DirectTarget 은 거리와 상관없이 100% */
	void Explode(const FVector& Center, const AActor* DirectTarget);

	/** 서버 전용. 대상이 피해를 받을 수 있으면 공격력 × 계수 × Scale 피해를 준다 */
	void ApplyOrbDamage(AActor* Target, const FHitResult& Hit, float Scale);

	/** 서버 전용. 시전자 ASC 로 적중 큐를 실행한다 */
	void PlayImpactCue(const FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	/** 꼬리 이펙트. 에셋은 BP 에서 지정(기본 비어 있음) */
	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UNiagaraComponent> TrailComp;

	/**
	 * 맞은 적이 낼 타격 큐 성질. 기본 GameplayCue.Hit.Magic.
	 * 맞는 순간에만 시전자에게 붙여 맞는 쪽 큐가 읽게 한다(HitCuePlay::FScopedImpactTag).
	 * 벽에 맞을 때는 이것과 별개로 OrbImpact 큐가 난다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	FGameplayTag ImpactTypeTag;

	TWeakObjectPtr<AHBPlayerCharacter> Shooter;
	float DamageCoef = 1.f;
	int32 CastId = 0;
	float ExplosionRadius = 0.f;
	float EdgeFraction = 1.f;

	/** 한 발에 한 번만 처리한다 */
	bool bHasHit = false;
};
