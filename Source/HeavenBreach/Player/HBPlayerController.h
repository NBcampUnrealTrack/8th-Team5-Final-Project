#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HBPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UInventoryUIComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHBPlayerController();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UInventoryUIComponent> InventoryUI;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void StartCrouch();
	void StopCrouch();
	void Attack();
	void Block();
	void ToggleInventory();
};
