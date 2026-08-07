#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/InventoryTypes.h"
#include "Item.generated.h"

UENUM(BlueprintType)
enum class EItemGrade : uint8
{
	Normal,
	Rare,
	Epic,
	Legend
};

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WeaponName;

	// Sword / Bow / Staff
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WeaponType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemGrade Grade = EItemGrade::Normal;

	// 이 아이템이 장착되는 슬롯 (무기는 기본 Weapon)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEquipSlotType EquipSlotType = EEquipSlotType::PrimaryWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AttackRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AttackInterval = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EquipMeshPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString IconPath;

	// 인벤토리 격자 크기 (가로 x 세로, 칸 단위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridHeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;
};

USTRUCT(BlueprintType)
struct FShieldData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ShieldName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemGrade Grade = EItemGrade::Normal;

	// 이 아이템이 장착되는 슬롯 (방패는 기본 Shield)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEquipSlotType EquipSlotType = EEquipSlotType::SecondaryEquip;

	// 0.0 ~ 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DamageReduction = 0.f;

	// degrees
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BlockAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EquipMeshPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString IconPath;

	// 인벤토리 격자 크기 (가로 x 세로, 칸 단위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridHeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;
};

USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ItemName;

	// Consumable / Currency / Junk / Bow / Staff
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ItemType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemGrade Grade = EItemGrade::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString UseEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WorldMeshPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString IconPath;

	// 이 아이템이 장착되는 슬롯. 장신구는 Accessory, 그 외(포션/골드/잡템/재료)는 None.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEquipSlotType EquipSlotType = EEquipSlotType::None;

	// 인벤토리 격자 크기 (가로 x 세로, 칸 단위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridHeight = 1;

	// Accessory stat bonus. Leave BonusStatType empty for items with no bonus (potions, gold, junk, materials).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BonusStatType;   // AttackPower / MaxHealth / MoveSpeed / DefenseRate / None

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BonusStatValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HealAmount = 0.f;
};

// One row per (Box, Item) pair - Row Name is "BoxID_ItemID" (e.g. BOX_001_ITM_001)
// because a single box can drop more than one possible item.
USTRUCT(BlueprintType)
struct FBoxLootData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BoxID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BoxName;

	// Matches the Row Name in the Item DataTable
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ItemID;

	// 0.0 ~ 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DropChance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxItemCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;
};
