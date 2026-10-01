#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTypes.h"
#include "EquipmentWidget.generated.h"

class UEquipmentComponent;
class UInventoryComponent;
class UEquipmentSlotWidget; // ���� ���� ���� ���� ����
class UInventoryWidget;

UCLASS()
class HEAVENBREACH_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void BindToEquipment(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory, UInventoryWidget* InOwningInventoryWidget = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RefreshSlots();

	UEquipmentSlotWidget* GetHoveredSlotWidget() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleEquipmentChanged();

	// -------------------------------------------------------------
	// WBP �������� ���� ������� 1:1�� ��Ī�Ǵ� BindWidget ����
	// (WBP�� ���� ����(Hierarchy) �̸��� �侾 �ϳ� Ʋ���� �ʰ� �Ȱ��ƾ� �մϴ�)
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

	// C++���� Enum ���� Ÿ������ ���� ������ ������ ã�� ��ȸ�ϱ� ���� TMap
	UPROPERTY()
	TMap<EEquipSlotType, TObjectPtr<UEquipmentSlotWidget>> SlotMap;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> BoundEquipment;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventory;

	UPROPERTY()
	TObjectPtr<UInventoryWidget> OwningInventoryWidget;

	void EnsureSlotMapInitialized();
};