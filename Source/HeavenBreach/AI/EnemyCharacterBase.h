// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Interaction/InteractableInterface.h"
#include "EnemyCharacterBase.generated.h"

class UEnemyAbilitySystemComponent;
class UEnemyAttributeSetBase;
class UEnemyArchetypeData;
class UGameplayEffect;
class ULootComponent;

UCLASS()
class HEAVENBREACH_API AEnemyCharacterBase : public ACharacter, public IAbilitySystemInterface, public IDamageableInterface, public IInteractableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ----- IDamageableInterface -----
	virtual bool CanBeDamaged_Implementation(AActor* InstigatorActor) const override;
	virtual void ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit) override;

	// ----- GAS -----
	// 빙의 될 시
	virtual void PossessedBy(AController* NewController) override;

	// AbilitySystemInterface로 오버라이드됨
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ----------

	//~ Begin IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	//~ End IInteractableInterface

	// true = 달리기, false = 걷기
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetMoveSpeed(bool bRun);

	// setter
	void SetWalkSpeed(float InWalkSpeed) { WalkSpeed = InWalkSpeed; }
	void SetRunSpeed(float InRunSpeed) { RunSpeed = InRunSpeed; }
	void SetAccRadius(float InAcceptanceRadius) { AcceptanceRadius = InAcceptanceRadius; }

	UFUNCTION(BlueprintCallable)
	void BecomeDead();

	// getter
	const UEnemyArchetypeData* GetArchetypeData() const { return ArchetypeData; }
	UEnemyAbilitySystemComponent* GetEnemyASC() { return AbilitySystemComp; }
	bool IsDead() const { return bDead; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 데미지 GE
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> IncomingDamageEffect;

	// character BP에서 데이터 관리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UEnemyArchetypeData> ArchetypeData;

	// GAS
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UEnemyAbilitySystemComponent> AbilitySystemComp;

	// 적 인벤토리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<ULootComponent> LootComponent;

	// GAS 스텟
	UPROPERTY()
	TObjectPtr<UEnemyAttributeSetBase> AttributeSet;

	// 걷는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;
	// 달리는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.f;
	// 목적지까지 도달 허용 범위
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 100.f;

	// 사망 확인
	UPROPERTY(ReplicatedUsing = OnRep_Dead, VisibleAnywhere, BlueprintReadOnly)
	bool bDead = false;

	// 사망변수가 변경되면 해야할것들
	UFUNCTION()
	void OnRep_Dead();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
