// Copyright RelicsOfTheFallenHeaven. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InventoryTypes.generated.h"

class UTexture2D;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EEquipSlotType : uint8
{
	None			UMETA(DisplayName = "None"),
	PrimaryWeapon	UMETA(DisplayName = "Primary Weapon"),
	SecondaryEquip	UMETA(DisplayName = "Secondary Equipment"),
	Helmet			UMETA(DisplayName = "Helmet"),
	Chest			UMETA(DisplayName = "Chest"),
	Legs			UMETA(DisplayName = "Legs"),
	Accessory		UMETA(DisplayName = "Accessory"),
	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EItemGrade : uint8
{
	Normal,
	Rare,
	Epic,
	Legend,
	Relic,
	Ancient
};

USTRUCT(BlueprintType)
struct FItemViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FIntPoint GridSize = FIntPoint(1, 1);
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EEquipSlotType EquipSlotType = EEquipSlotType::None;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 MaxStack = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EItemGrade Grade = EItemGrade::Normal;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FString IconPath;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 SellValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	bool IsStackable() const { return MaxStack > 1; }
	bool IsEquippable() const { return EquipSlotType != EEquipSlotType::None; }
	UPROPERTY(BlueprintReadOnly)
	float HealAmount = 0.f;
	bool IsConsumable() const { return HealAmount > 0.f; }

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	bool bIsRelic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	FString BenefitDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	FString RiskDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> BenefitEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> RiskEffect;
};

USTRUCT(BlueprintType)
struct FItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item")
	FDataTableRowHandle ItemRow;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item", meta = (ClampMin = 1))
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid OriginalOwnerId;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bRotated = false;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bIsFavorite = false;

	// 장비 내구도. -1이면 "이 인스턴스는 아직 내구도가 초기화되지 않음"을 의미하고,
	// 제작/파밍으로 아이템이 처음 생성되는 시점에 등급별 최대치로 채워줘야 한다.
	// 0이 되면 파손 상태(장착/전투 사용/재각인 불가, 보관/판매/분해만 가능).
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float CurrentDurability = -1.f;

	bool IsBroken() const { return CurrentDurability == 0.f; }

	bool IsValid() const;

	bool operator==(const FItemInstance& Other) const
	{
		return InstanceId == Other.InstanceId;
	}
};

USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FItemInstance Item;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint Position = FIntPoint::ZeroValue;
};

HEAVENBREACH_API bool ResolveItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData);

HEAVENBREACH_API FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData);

// 등급별 최대 내구도(기획서 10.2 기준: 일반 100 / 희귀 150 / 영웅 200 / 천상 250).
// 밸런스 조정은 여기 한 곳만 고치면 된다.
HEAVENBREACH_API float GetMaxDurabilityForGrade(EItemGrade InGrade);

// 등급별 배경 그라데이션 색상(인벤토리 칸/장착 슬롯 공용). 색 조정은 여기 한 곳만
// 고치면 InventoryItemWidget, EquipmentSlotWidget 양쪽에 다 반영된다.
HEAVENBREACH_API FLinearColor GetGradeTopColor(EItemGrade InGrade);
HEAVENBREACH_API FLinearColor GetGradeBottomColor(EItemGrade InGrade);

HEAVENBREACH_API UTexture2D* LoadItemIcon(const FString& InIconPath);