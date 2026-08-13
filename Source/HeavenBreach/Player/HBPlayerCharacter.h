#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Interface/DamageableInterface.h"
#include "GameFramework/Character.h"
#include "HBPlayerCharacter.generated.h"

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

	void MoveForward(float Value);
	void MoveRight(float Value);

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void Attack();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StartBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StopBlocking();

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

protected:
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

};
