#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "EquipmentComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent();

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentChanged OnEquipmentChanged;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsSlotOccupied(EEquipSlotType InSlot) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	const TArray<FItemInstance>& GetSlots() const { return Slots; }

	bool GetItemInSlot(EEquipSlotType InSlot, FItemInstance& OutItem) const;

	bool FindSlotByInstanceId(FGuid InInstanceId, EEquipSlotType& OutSlot) const;

	bool CanEquip(const FItemInstance& InItem, EEquipSlotType InSlot) const;

	bool EquipFromInventory(UInventoryComponent* InSource, FGuid InInstanceId, EEquipSlotType InSlot);

	bool UnequipToInventory(EEquipSlotType InSlot, UInventoryComponent* InTarget);

	bool UnequipToInventoryAt(EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition);

	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	void DebugPrintSlots() const;

protected:
	virtual void BeginPlay() override;

private:
	static int32 SlotToIndex(EEquipSlotType InSlot) { return static_cast<int32>(InSlot); }

	UPROPERTY()
	TArray<FItemInstance> Slots;
};