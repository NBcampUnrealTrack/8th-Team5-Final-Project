#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Interface/DamageableInterface.h"
#include "GameFramework/Character.h"
#include "HBPlayerCharacter.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class AHB_PlayerState;
class UCameraComponent;
class UInventoryComponent;
class UEquipmentComponent;
class UInteractionComponent;
class UInteractionPromptWidget;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerCharacter : public ACharacter,public IDamageableInterface,public IAbilitySystemInterface

{
	GENERATED_BODY()

public:
	AHBPlayerCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void Attack();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StartBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StopBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void ToggleBlocking();

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UInventoryComponent* GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UEquipmentComponent* GetEquipment() const { return Equipment; }

#pragma region Interaction
public:
	virtual void PawnClientRestart() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Player|UI")
	TSubclassOf<UInteractionPromptWidget> InteractionPromptClass;

private:
	UPROPERTY()
	TObjectPtr<UInteractionPromptWidget> InteractionPromptWidgetInstance;

	void SetupInteractionPrompt();

	UFUNCTION()
	void HandleInteractableFocusChanged(AActor* NewInteractable);
#pragma endregion

private:
	UFUNCTION()
	void HandleItemUsed(FName ItemId, float HealAmount);

protected:
	virtual void OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UEquipmentComponent> Equipment;
	
#pragma region ASC
public:
	AHB_PlayerState* GetPlayerCurrentState() const
	{
		return HB_PlayerState ? HB_PlayerState : nullptr;
	}
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
private:
	UPROPERTY()
	TObjectPtr<AHB_PlayerState> HB_PlayerState;
#pragma endregion 
	
#pragma region DamageInterface
	
public:
	virtual bool CanBeDamaged_Implementation(AActor* InstigatorActor) const override;
	
	virtual void ApplyMeleeHit_Implementation(AActor* Instigator, float RawDamage, const FHitResult& Hit) override;
	
#pragma endregion 
	
#pragma region CombatSystem
public:
	//TODO:: 데이터테이블, 구조체로 변경
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackPower() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackRange() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockDamageReduction() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockAngle() const;
	
protected:
	/****GA***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
	
	/****GE***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TSubclassOf<UGameplayEffect> GE_PlayerDamage;
	
	/****CameraShake***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TSubclassOf<UCameraShakeBase> CS_HitReact;
	
	/****BaseAbilities Stat Value***/
	// TODO:: Temp변수 데이터테이블로 변경 or AttributeData변경
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackPower = 10.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackRange = 150.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseBlockDamageReduction = 0.3f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseBlockingAngle = 120.f;
#pragma endregion 
	
#pragma region Movement
public:
	void StartSprint();
	
	void StopSprint();
protected:
	UPROPERTY(EditDefaultsOnly,Category="Player|Movement")
	float WalkSpeed = 300.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Movement")
	float SprintSpeed = 600.f;
	
	UPROPERTY(ReplicatedUsing= OnRep_IsSprinting)
	bool bIsSprinting = false;
	
	UFUNCTION()
	void OnRep_IsSprinting();
	
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);
#pragma endregion 
	
#pragma region EquimentVisual
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComp;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<UStaticMeshComponent> ShieldMeshComp;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<UStaticMeshComponent> HelmetMeshComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> ChestMeshComp;

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName WeaponSocketName = TEXT("PrimaryWeaponSocket");
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName ShieldSocketName = TEXT("SecondWeaponSocket");
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName HelmetSocketName = TEXT("HelmetSocket");
	
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void RefreshEquippedVisuals();
	
	void RefreshWeaponVisual();
	void RefreshShieldVisual();
	void RefreshHelmetVisual();
	void RefreshChestVisual();

#pragma endregion 

public:
	UFUNCTION(Exec, Category = "Debug")
	void TestDamage(float Amount);

protected:
	// C++에서 호출하면 블루프린트에서 실행되는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_OnPlayerStateReady();
};
