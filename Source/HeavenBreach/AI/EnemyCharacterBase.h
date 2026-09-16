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
struct FGameplayTag;
struct FDataTableRowHandle;
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
	virtual void ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit, EHBDamageSource Source) override;

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
	void SetWalkSpeed(float InWalkSpeed) { WalkSpeed = InWalkSpeed; RefreshMoveSpeed(); }
	void SetRunSpeed(float InRunSpeed) { RunSpeed = InRunSpeed; RefreshMoveSpeed(); }
	void SetAccRadius(float InAcceptanceRadius) { AcceptanceRadius = InAcceptanceRadius; }
	void SetReward(float InExpReward) { ExpReward = InExpReward; }

	// 드랍 테이블의 행을 루트 컴포넌트에 전달한다.
	// 스탯 DT 에서 주입되므로 적 BP 마다 따로 설정하지 않는다.
	void SetLootRow(const FDataTableRowHandle& InRow);

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

	// 뼈대 별로 데미지 배율. 여기에 없다면 1배
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TMap<FName, float> BoneDamageMultipliers;

	// 걷는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;
	// 달리는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.f;
	// 목적지까지 도달 허용 범위
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 100.f;
	// 받아온 이동속도 배율 값
	UPROPERTY(BlueprintReadOnly, Category = "AI|Movement")
	float CachedSpeedRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	float ExpReward = 10.f;

	// 사망 확인
	UPROPERTY(ReplicatedUsing = OnRep_Dead, VisibleAnywhere, BlueprintReadOnly)
	bool bDead = false;

	// 달리기 확인
	bool bRunning = false;

	// 마지막으로 자신을공격한 대상
	TWeakObjectPtr<AActor> LastDamageInstigator;

	// 사망변수가 변경되면 해야할것들
	UFUNCTION()
	void OnRep_Dead();

	// 이동속도 변경 적용
	void RefreshMoveSpeed();

	// GAS에서 이동속도 변경 시 연결된 함수
	void OnMoveSpeedRateChanged(const struct FOnAttributeChangeData& Data);

	// 죽인대상에게 보상
	void GiveExpToKiller();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
