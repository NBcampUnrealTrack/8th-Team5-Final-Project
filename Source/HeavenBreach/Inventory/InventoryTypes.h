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

HEAVENBREACH_API UTexture2D* LoadItemIcon(const FString& InIconPath);