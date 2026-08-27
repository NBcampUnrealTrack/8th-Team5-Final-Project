#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTypes.h"
#include "EquipmentWidget.generated.h"

class UEquipmentComponent;
class UInventoryComponent;
class UEquipmentSlotWidget; // 개별 슬롯 위젯 전방 선언

UCLASS()
class HEAVENBREACH_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void BindToEquipment(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RefreshSlots();

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleEquipmentChanged();

	// -------------------------------------------------------------
	// WBP 에디터의 슬롯 위젯들과 1:1로 매칭되는 BindWidget 선언
	// (WBP의 계층 구조(Hierarchy) 이름과 토씨 하나 틀리지 않고 똑같아야 합니다)
	// -------------------------------------------------------------
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_Helmet;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_Chest;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_Legs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_PrimaryWeapon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_SecondaryEquip;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEquipmentSlotWidget> Slot_Accessory;

private:

	// C++에서 Enum 슬롯 타입으로 슬롯 위젯을 빠르게 찾아 순회하기 위한 TMap
	UPROPERTY()
	TMap<EEquipSlotType, TObjectPtr<UEquipmentSlotWidget>> SlotMap;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> BoundEquipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	void EnsureSlotMapInitialized();
};