#include "InventoryTypes.h"
#include "Item.h"

namespace
{
	const FString ResolveContext(TEXT("ResolveItemViewData"));
}

bool FItemInstance::IsValid() const
{
	return !ItemRow.IsNull() && InstanceId.IsValid() && StackCount > 0;
}

bool ResolveItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData)
{
	if (InRow.IsNull())
	{
		return false;
	}

	if (const FItemData* ItemRow = InRow.GetRow<FItemData>(ResolveContext))
	{
		OutViewData.GridSize = FIntPoint(ItemRow->GridWidth, ItemRow->GridHeight);
		OutViewData.EquipSlotType = ItemRow->EquipSlotType;
		OutViewData.MaxStack = FMath::Max(1, ItemRow->MaxStack);
		OutViewData.DisplayName = ItemRow->ItemName;
		OutViewData.IconPath = ItemRow->IconPath;
		return true;
	}

	if (const FWeaponData* WeaponRow = InRow.GetRow<FWeaponData>(ResolveContext))
	{
		OutViewData.GridSize = FIntPoint(WeaponRow->GridWidth, WeaponRow->GridHeight);
		OutViewData.EquipSlotType = WeaponRow->EquipSlotType;
		OutViewData.MaxStack = 1;
		OutViewData.DisplayName = WeaponRow->WeaponName;
		OutViewData.IconPath = WeaponRow->IconPath;
		return true;
	}

	if (const FShieldData* ShieldRow = InRow.GetRow<FShieldData>(ResolveContext))
	{
		OutViewData.GridSize = FIntPoint(ShieldRow->GridWidth, ShieldRow->GridHeight);
		OutViewData.EquipSlotType = ShieldRow->EquipSlotType;
		OutViewData.MaxStack = 1;
		OutViewData.DisplayName = ShieldRow->ShieldName;
		OutViewData.IconPath = ShieldRow->IconPath;
		return true;
	}

	return false;
}

FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData)
{
	if (InItem.bRotated)
	{
		return FIntPoint(InViewData.GridSize.Y, InViewData.GridSize.X);
	}

	return InViewData.GridSize;
}