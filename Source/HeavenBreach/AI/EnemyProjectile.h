// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;


UCLASS()
class HEAVENBREACH_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEnemyProjectile();


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 발사하는 함수. 발사시킨 액터와 데미지량을 넣어야 한다. 
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Launch(AActor* InInstigator, float InDamage = -1.f);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// ----- 컴포넌트 -----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	// 속도와 낙하는 이 컴포넌트의 디테일에서 직접 조절한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	// ----- 발사 설정 -----

	// 피해량
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float Damage = 10.f;

	// 피해 배율
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float DamageMultiplier = 1.f;

	// 맞았을 때 박힐지 사라질지
	// true : 박히고 일정기간 삼. false : 바로 사라짐
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	bool HitBehavior = false;

	// 박힌 뒤 남아 있는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float StickLifeSpan = 5.f;

	// 아무것도 안 맞았을 때의 수명. 혹시 모르니 아무도안맞은채로 날아갈시 스스로 사라질 시간
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifeSpan = 20.f;

	// 명중 여부
	UPROPERTY(ReplicatedUsing = OnRep_Hit)
	bool bHit = false;

	UFUNCTION()
	void OnRep_Hit();
};
