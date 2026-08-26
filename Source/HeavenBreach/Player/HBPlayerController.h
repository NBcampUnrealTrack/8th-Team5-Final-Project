#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HBPlayerController.generated.h"

class UCommonActivatableWidget;
class UHB_UILayout;
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

	UFUNCTION(BlueprintCallable, Category = "Player|Inventory")
	void RequestCloseInventory();

	// Root CommonUI widget. Every screen is pushed into one of its layers.
	UFUNCTION(BlueprintPure, Category = "Player|UI")
	UHB_UILayout* GetUILayout() const { return UILayout; }

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
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UInventoryUIComponent> InventoryUI;

	// WBP_UILayout
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UHB_UILayout> UILayoutClass;

	// WBP_MainHUD, pushed into the UI.Layer.Game stack on begin play
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UCommonActivatableWidget> HUDLayerClass;

	UPROPERTY(Transient)
	TObjectPtr<UHB_UILayout> UILayout;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void CreateUILayout();

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
	void StartSprint();
	void StopSprint();
	
#pragma region CameraShake
public:
	UFUNCTION(Client,Reliable)
	void ClientPlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass);
#pragma endregion 
};
