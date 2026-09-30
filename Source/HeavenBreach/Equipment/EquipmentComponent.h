#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "AbilitySystemComponent.h"
#include "Item/ItemStatBinding.h"
#include "Item/ItemEffectRules.h"
#include "EquipmentComponent.generated.h"

class UInventoryComponent;
class UAbilitySystemComponent;
struct FItemOption;
struct FItemEffect;
struct FWeaponData;

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

	// 지금 주무기로 활/지팡이(양손무기)가 장착되어 있어서 보조 장비 슬롯을 아예 쓸 수 없는
	// 상태인지. UI가 보조 슬롯에 잠금 오버레이(자물쇠 아이콘 등)를 켤지 판단하는 용도 -
	// CanEquip과 같은 조건(IsShieldIncompatibleWeaponType)을 그대로 재사용한다.
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsSecondarySlotLockedByWeapon() const;

	bool EquipFromInventory(UInventoryComponent* InSource, FGuid InInstanceId, EEquipSlotType InSlot);

	bool UnequipToInventory(EEquipSlotType InSlot, UInventoryComponent* InTarget);

	bool UnequipToInventoryAt(EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	void DebugPrintSlots() const;

	void ApplyLoadedSlots(const TArray<FItemInstance>& InSlots);

	// OnEquipmentChanged 에 물린다. 숫자 옵션 GE 와 동작 효과 GE 를 둘 다 맞춘다. 서버 전용.
	//
	// 왜 한 곳인가: 전에는 유물 효과를 장착·해제 함수에서 손으로 붙였다 뗐는데,
	// 세이브 복원(ApplyLoadedSlots)은 그 함수를 안 거쳐서 복원 후 효과가 사라졌다.
	// 델리게이트 하나에 물려 두면 장착·해제·교체·파손·복원이 전부 같은 길을 탄다.
	UFUNCTION()
	void HandleEquipmentChanged();

	// 지금 낀 장비의 동작 효과를 정의(DT_ItemEffect)와 합쳐 모은다. 부서진 장비는 뺀다.
	// 같은 효과를 두 개 꼈으면 먼저 찾은 것만 — 겹치지 않는다.
	// ItemEffectRules 가 틱마다 · 스킬 쓸 때마다 이걸로 무엇을 할지 정한다.
	void CollectEquippedEffects(TArray<FResolvedItemEffect>& OutEffects) const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Slots();

private:
	static int32 SlotToIndex(EEquipSlotType InSlot) { return static_cast<int32>(InSlot); }

	class UAbilitySystemComponent* GetOwnerASC() const;

	// 숫자 옵션 → UGE_EquipmentStats 하나. 전부 다시 세서 통째로 떼고 붙인다.
	//
	// 왜 전부 다시 세나: 슬롯별로 GE 핸들을 들면 교체·파손·세이브 복원마다
	// 어느 핸들을 떼고 붙일지가 갈라지고, 한 경로를 빠뜨리면 스탯이 남거나 사라진다.
	// 통째로 다시 계산하면 경로가 하나뿐이라 그 실수가 불가능하다.
	// 쳐낼 조건: 장비 수가 수십 개가 되어 재계산이 눈에 띄면 슬롯별 핸들로 쪼갠다.
	void RefreshEquipmentStats();

	// 동작 효과 → UGE_ItemEffectTag 하나에 태그를 전부 담는다. 핸들도 하나.
	void RefreshItemEffects();

	// 장착한 모든 칸의 스탯 합계. 무기 공격력 + 방어구 방어력 + 모든 칸의 숫자 옵션.
	FItemStatTotals CollectStatTotals() const;

	// 장착한 모든 칸의 동작 효과 "꼈다" 태그. 같은 태그는 한 번만 담는다.
	TArray<FGameplayTag> CollectItemEffectTags() const;

	// 옵션 목록을 합계에 쌓는다. 표에 없는 스탯이나 받지 않는 타입은 경고를 찍고 건너뛴다.
	void AddOptionsTo(const TArray<FItemOption>& Options, FName SourceRow, FItemStatTotals& Totals) const;

	// 효과 줄을 정의와 합쳐 쌓는다. 정의를 못 찾거나 정의에 Tag 가 없으면 경고를 찍고 건너뛴다.
	void AddResolvedEffects(const TArray<FItemEffect>& Effects, FName SourceRow, TArray<FResolvedItemEffect>& OutEffects) const;

	static bool HasResolvedEffect(const TArray<FResolvedItemEffect>& Effects, const FGameplayTag& Tag);
	static bool AreTotalsEqual(const FItemStatTotals& A, const FItemStatTotals& B);
	static bool AreSameTags(const TArray<FGameplayTag>& A, const TArray<FGameplayTag>& B);

	// 부서지지 않은 투구/상의/하의의 BonusStatValue 합.
	float SumArmorDefense() const;

	// 부서지지 않은 주무기의 FWeaponData::AttackPower.
	float SumWeaponAttackPower() const;

	// 부서지지 않은 주무기의 공격 속도 배율. 무기가 없거나 부서졌으면 1(영향 없음).
	float GetWeaponAttackSpeedRate() const;

	// 공격에 쓸 수 있는 주무기의 행. 없거나 부서졌으면 nullptr.
	// 무기 기초 스탯이 둘이 되어서 "부서지면 0" 규칙을 한 곳에 모았다.
	const FWeaponData* FindUsableWeaponRow() const;

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FItemInstance> Slots;

	// 장비 보정 GE 의 핸들. 하나만 든다 — 슬롯이 바뀌면 통째로 새로 만든다.
	UPROPERTY()
	FActiveGameplayEffectHandle EquipmentStatsHandle;

	// 마지막으로 GE 에 실어 보낸 합계. 같으면 다시 붙이지 않는다.
	//
	// 왜 필요한가: 내구도는 피격마다 방어구 세 칸이 깎이는데 합계는 그대로다.
	// 그때마다 GE 를 떼고 붙이면 어트리뷰트가 67 → 0 → 67 로 튄다. 스탯 UI 가 붙으면
	// 맞을 때마다 숫자가 깜빡인다.
	FItemStatTotals LastAppliedTotals;

	// 동작 효과 GE 의 핸들. 하나만 든다 — 스탯 GE 와 같은 이유.
	UPROPERTY()
	FActiveGameplayEffectHandle ItemEffectHandle;

	// 지금 붙어 있는 효과 태그. 새로 센 목록과 같으면 다시 붙이지 않는다 (내구도만 깎인 경우).
	TArray<FGameplayTag> AppliedItemEffectTags;
};