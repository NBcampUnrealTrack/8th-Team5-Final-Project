#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryUIComponent.generated.h"

class UHB_UILayout;
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

	/** 인벤토리가 Menu 레이어에 올라가 있는지. 스택 상태가 곧 열림 상태다 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryOpen() const { return InventoryWidget != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OpenStash();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseStash();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UUserWidget> StashWidgetClass;

private:
	/** 소유 컨트롤러의 UI 레이아웃. 없으면 nullptr */
	UHB_UILayout* GetUILayout() const;

	/** Menu 레이어에 올라가 있는 인스턴스. nullptr이면 닫힌 상태 */
	UPROPERTY()
	TObjectPtr<UInventoryWidget> InventoryWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> StashWidget;
};