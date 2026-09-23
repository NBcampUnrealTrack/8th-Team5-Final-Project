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

	// 장착 장비가 주는 어트리뷰트 보정을 통째로 다시 계산해 붙인다. 서버 전용.
	//
	// 왜 전부 다시 세나: 슬롯별로 GE 핸들을 들면 교체·파손·세이브 복원마다
	// 어느 핸들을 떼고 붙일지가 갈라지고, 한 경로를 빠뜨리면 방어력이 남거나 사라진다.
	// 통째로 다시 계산하면 경로가 하나뿐이라 그 실수가 불가능하다.
	//
	// BeginPlay 에서 OnEquipmentChanged 에 물려 두므로 직접 부를 일은 없다.
	// 쳐낼 조건: 장비 수가 수십 개가 되어 재계산이 눈에 띄면 슬롯별 핸들로 쪼갠다.
	UFUNCTION()
	void RefreshEquipmentStats();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Slots();

private:
	static int32 SlotToIndex(EEquipSlotType InSlot) { return static_cast<int32>(InSlot); }

	class UAbilitySystemComponent* GetOwnerASC() const;

	void ApplyRelicEffects(const FItemInstance& InItem);

	void RemoveRelicEffects();

	// 부서지지 않은 투구/상의/하의의 BonusStatValue 합.
	float SumArmorDefense() const;

	// 부서지지 않은 주무기의 FWeaponData::AttackPower.
	float SumWeaponAttackPower() const;

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FItemInstance> Slots;

	UPROPERTY()
	FActiveGameplayEffectHandle EquippedRelicBenefitHandle;

	UPROPERTY()
	FActiveGameplayEffectHandle EquippedRelicRiskHandle;

	// 장비 보정 GE 의 핸들. 하나만 든다 — 슬롯이 바뀌면 통째로 새로 만든다.
	UPROPERTY()
	FActiveGameplayEffectHandle EquipmentStatsHandle;

	// 마지막으로 GE 에 실어 보낸 값. 같으면 다시 붙이지 않는다.
	//
	// 왜 필요한가: 내구도는 피격마다 방어구 세 칸이 깎이는데 방어력 합계는 그대로다.
	// 그때마다 GE 를 떼고 붙이면 어트리뷰트가 67 → 0 → 67 로 튄다. 지금은 이 값을
	// 구독하는 코드가 없어 무해하지만, 스탯 UI 가 붙는 순간 맞을 때마다 숫자가 깜빡인다.
	//
	// 쳐낼 조건: 장비에 옵션이 붙어 합계가 구조체가 되면 비교도 그 구조체로 옮긴다.
	float LastAppliedDefense = -1.f;
	float LastAppliedAttackPower = -1.f;
};