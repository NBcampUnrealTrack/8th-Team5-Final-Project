#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InventoryTypes.generated.h"

class UItemDefinition;
class UTexture2D;

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
enum class EItemCategory : uint8
{
	Equipment			UMETA(DisplayName = "Equipment"),
	Consumable			UMETA(DisplayName = "Consumable"),
	CraftingMaterial	UMETA(DisplayName = "Crafting Material"),
	CoreMaterial		UMETA(DisplayName = "Core Material"),
	Currency			UMETA(DisplayName = "Currency"),
	Valuable			UMETA(DisplayName = "Valuable")
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
	EItemCategory Category = EItemCategory::Valuable;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 MaxStack = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	bool IsStackable() const { return MaxStack > 1; }
	bool IsEquippable() const { return EquipSlotType != EEquipSlotType::None; }
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

USTRUCT(BlueprintType)
struct FMockItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Item", meta = (ClampMin = 1))
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, Category = "Item")
	EEquipSlotType EquipSlotType = EEquipSlotType::None;

	UPROPERTY(EditAnywhere, Category = "Item")
	EItemCategory Category = EItemCategory::Valuable;

	UPROPERTY(EditAnywhere, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;
};

HEAVENBREACH_API bool ResolveItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData);

HEAVENBREACH_API FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData);