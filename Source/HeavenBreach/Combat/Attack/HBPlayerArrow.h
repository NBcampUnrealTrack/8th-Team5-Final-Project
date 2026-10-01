// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBPlayerArrow.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class AHBPlayerCharacter;

/**
 * 활 기본공격 화살. 서버가 스폰하고 서버만 피해를 준다.
 * 피해 순서는 근접 판정(UMeleeTraceComponent::ApplyDamage)과 같다. 한 발은 한 번만 맞는다.
 */
UCLASS()
class HEAVENBREACH_API AHBPlayerArrow : public AActor
{
	GENERATED_BODY()

public:
	AHBPlayerArrow();

	/** 서버 전용. 쏜 사람·속도·피해 계수를 정하고 날린다 */
	void Launch(AHBPlayerCharacter* InShooter, float InSpeed, float InDamageCoef);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** 대상에 피해를 준다. 벽·바닥이면 아무 일도 없다 */
	void ApplyArrowDamage(AActor* Target, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	/** 중력 배율. 멀리 쏘면 살짝 떨어진다 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow", meta = (ClampMin = "0.0"))
	float GravityScale = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Arrow", meta = (ClampMin = "0.1", Units = "s"))
	float ArrowLifeSpan = 5.f;

private:
	UPROPERTY()
	TObjectPtr<AHBPlayerCharacter> Shooter;

	float DamageCoef = 1.f;

	/** 한 발 = 한 번 명중. 같은 프레임에 두 번 들어와도 한 번만 처리한다 */
	bool bHasHit = false;
};
