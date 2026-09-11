#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "AbilitySystemComponent.h"
#include "EquipmentComponent.generated.h"

class UInventoryComponent;
class UAbilitySystemComponent;

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

	// 장착된 아이템의 내구도를 InAmount만큼 깎는다. 서버(Authority)에서만 동작한다.
	// 이번 호출로 막 0에 도달했으면 bOutJustBroke가 true로 나온다(파손 연출/알림용).
	// 이미 파손된 상태거나 슬롯이 비어있으면 false를 반환한다.
	bool DamageEquippedItemDurability(EEquipSlotType InSlot, float InAmount, bool& bOutJustBroke);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsEquippedItemBroken(EEquipSlotType InSlot) const;

	bool FindSlotByInstanceId(FGuid InInstanceId, EEquipSlotType& OutSlot) const;

	bool CanEquip(const FItemInstance& InItem, EEquipSlotType InSlot) const;

	bool EquipFromInventory(UInventoryComponent* InSource, FGuid InInstanceId, EEquipSlotType InSlot);

	bool UnequipToInventory(EEquipSlotType InSlot, UInventoryComponent* InTarget);

	bool UnequipToInventoryAt(EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	void DebugPrintSlots() const;

	void ApplyLoadedSlots(const TArray<FItemInstance>& InSlots);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Slots();

private:
	static int32 SlotToIndex(EEquipSlotType InSlot) { return static_cast<int32>(InSlot); }

	class UAbilitySystemComponent* GetOwnerASC() const;

	void ApplyRelicEffects(const FItemInstance& InItem);

	void RemoveRelicEffects();

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FItemInstance> Slots;

	UPROPERTY()
	FActiveGameplayEffectHandle EquippedRelicBenefitHandle;

	UPROPERTY()
	FActiveGameplayEffectHandle EquippedRelicRiskHandle;
};