#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryUIComponent.generated.h"

class UInventoryWidget;

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInventoryUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryUIComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OpenInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryOpen() const { return bIsOpen; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

private:
	void EnsureWidget();

	UPROPERTY()
	TObjectPtr<UInventoryWidget> InventoryWidget;

	bool bIsOpen = false;
};