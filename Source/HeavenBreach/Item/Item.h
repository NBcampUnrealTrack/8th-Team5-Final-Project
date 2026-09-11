#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/InventoryTypes.h"
#include "Item.generated.h"

class UTexture2D;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WeaponName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WeaponType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemGrade Grade = EItemGrade::Normal;
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
	TObjectPtr<UTexture2D> Icon;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEquipSlotType EquipSlotType = EEquipSlotType::SecondaryEquip;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DamageReduction = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BlockAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EquipMeshPath;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon;
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
	TObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEquipSlotType EquipSlotType = EEquipSlotType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridWidth = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridHeight = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BonusStatType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BonusStatValue = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HealAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ShoesMeshPath;

	// ===== 장신구(Relic) 전용 필드 =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	bool bIsRelic = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	FString BenefitDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	FString RiskDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> BenefitEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> RiskEffect;
};
USTRUCT(BlueprintType)
struct FBoxLootData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BoxID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BoxName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ItemID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DropChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxItemCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Notes;
};