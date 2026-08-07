#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HBPlayerCharacter.generated.h"

class AHB_PlayerState;
class UCameraComponent;
class UInventoryComponent;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerCharacter : public ACharacter
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
#pragma region ASC
public:
	AHB_PlayerState* GetPlayerCurrentState() const
	{
		if (HB_PlayerState)
		{
			return HB_PlayerState;
		}
		return nullptr;
	}
protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
private:
	UPROPERTY()
	TObjectPtr<AHB_PlayerState> HB_PlayerState;
#pragma endregion 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UInventoryComponent> Inventory;
};
