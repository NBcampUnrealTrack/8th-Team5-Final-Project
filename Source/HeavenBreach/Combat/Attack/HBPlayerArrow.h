// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Attack/HBArrowStuck.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "HBPlayerArrow.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class AHBPlayerCharacter;

/**
 * 활 기본공격 화살. 서버가 스폰하고 서버만 피해를 준다.
 * 피해 순서는 근접 판정(UMeleeTraceComponent::ApplyDamage)과 같다. 한 발은 한 번만 맞는다.
 * 맞으면 그 자리에 박힌다. 벽·사물에 박힌 화살은 F 로 주워 가방에 1개 돌려받는다.
 * 몬스터·플레이어에 박힌 화살은 회수할 수 없고 잠시 뒤 사라진다.
 */
UCLASS()
class HEAVENBREACH_API AHBPlayerArrow : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AHBPlayerArrow();

	virtual void PostInitializeComponents() override;

	/** 서버 전용. 쏜 사람·속도·피해 계수·회수 시 돌려줄 아이템 행을 정하고 날린다 */
	void Launch(AHBPlayerCharacter* InShooter, float InSpeed, float InDamageCoef, const FDataTableRowHandle& InAmmoRow);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** 대상에 피해를 준다. 벽·바닥이면 아무 일도 없다 */
	void ApplyArrowDamage(AActor* Target, const FHitResult& Hit);

	/** 서버 전용. 맞은 곳에 박는다. 붙일 곳이 없으면 바로 사라진다 */
	void StickTo(AActor* HitActor, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_StuckState();

	/** 박혀 있던 몬스터·플레이어가 사라지면 같이 사라진다 */
	UFUNCTION()
	void OnStuckActorDestroyed(AActor* DestroyedActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	/** 날아가는 궤적. 에셋은 BP 에서 넣는다(비어 있으면 아무것도 안 보임). 박히면 꺼진다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> TrailComp;

	/** 벽·사물에 박혔을 때만 켜지는 줍기 판정. 화살이 가늘어서 상호작용 트레이스가 잘 맞게 키운다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> PickupSphere;

	/** 중력 배율. 멀리 쏘면 살짝 떨어진다 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow", meta = (ClampMin = "0.0"))
	float GravityScale = 0.2f;

	/** 아무것도 안 맞고 날아갈 때 사라지기까지 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow", meta = (ClampMin = "0.1", Units = "s"))
	float ArrowLifeSpan = 5.f;

	/** 벽·사물에 박힌 뒤 사라지기까지. 이 안에 F 로 주울 수 있다 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Stick", meta = (ClampMin = "0.1", Units = "s"))
	float WorldStickLifeSpan = 120.f;

	/** 몬스터·플레이어에 박힌 뒤 사라지기까지 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Stick", meta = (ClampMin = "0.1", Units = "s"))
	float CharacterStickLifeSpan = 10.f;

	/** 맞은 지점에서 날아가던 방향으로 이만큼 더 밀어 넣어 박힌 것처럼 보이게 한다 */
	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Stick", meta = (ClampMin = "0.0", Units = "cm"))
	float StickPenetration = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Pickup", meta = (ClampMin = "1.0", Units = "cm"))
	float PickupRadius = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Pickup")
	FText PickupPrompt = NSLOCTEXT("PlayerArrow", "PickupPrompt", "화살 줍기");

	UPROPERTY(EditDefaultsOnly, Category = "Arrow|Pickup")
	FText BagFullMessage = NSLOCTEXT("PlayerArrow", "BagFull", "가방이 가득 찼습니다");

private:
	UPROPERTY()
	TObjectPtr<AHBPlayerCharacter> Shooter;

	/** 주웠을 때 가방에 넣을 아이템. 쏠 때 GA 가 넘긴다. 클라도 줍기 가능 여부를 봐야 해서 복제 */
	UPROPERTY(Replicated)
	FDataTableRowHandle AmmoItemRow;

	UPROPERTY(ReplicatedUsing = OnRep_StuckState)
	FHBArrowStuckState StuckState;

	float DamageCoef = 1.f;

	/** 한 발 = 한 번 명중. 같은 프레임에 두 번 들어와도 한 번만 처리한다 */
	bool bHasHit = false;

	/** 두 사람이 같은 프레임에 주워도 한 번만 준다 */
	bool bPickedUp = false;
};
