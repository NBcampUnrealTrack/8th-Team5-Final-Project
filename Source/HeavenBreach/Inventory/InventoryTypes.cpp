#include "InventoryTypes.h"

#include "Item/Item.h"
#include "Engine/Texture2D.h"


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
	if (InRow.IsNull() || InRow.DataTable == nullptr)
	{
		return false;
	}

	const UScriptStruct* RowStruct = InRow.DataTable->GetRowStruct();
	if (RowStruct == nullptr)
	{
		return false;
	}

	if (RowStruct == FItemData::StaticStruct())
	{
		const FItemData* ItemRow = InRow.GetRow<FItemData>(ResolveContext);
		if (ItemRow == nullptr)
		{
			return false;
		}

		OutViewData.GridSize = FIntPoint(ItemRow->GridWidth, ItemRow->GridHeight);
		OutViewData.EquipSlotType = ItemRow->EquipSlotType;
		OutViewData.MaxStack = FMath::Max(1, ItemRow->MaxStack);
		OutViewData.DisplayName = ItemRow->ItemName;
		OutViewData.IconPath = ItemRow->Icon ? ItemRow->Icon->GetPathName() : FString();
		OutViewData.HealAmount = ItemRow->HealAmount;
		OutViewData.SellValue = ItemRow->SellValue;
		OutViewData.Icon = ItemRow->Icon;
		return true;
	}

	if (RowStruct == FWeaponData::StaticStruct())
	{
		const FWeaponData* WeaponRow = InRow.GetRow<FWeaponData>(ResolveContext);
		if (WeaponRow == nullptr)
		{
			return false;
		}

		OutViewData.GridSize = FIntPoint(WeaponRow->GridWidth, WeaponRow->GridHeight);
		OutViewData.EquipSlotType = WeaponRow->EquipSlotType;
		OutViewData.MaxStack = 1;
		OutViewData.DisplayName = WeaponRow->WeaponName;
		OutViewData.IconPath = WeaponRow->Icon ? WeaponRow->Icon->GetPathName() : FString();
		OutViewData.SellValue = WeaponRow->SellValue;
		OutViewData.Icon = WeaponRow->Icon;
		return true;
	}

	if (RowStruct == FShieldData::StaticStruct())
	{
		const FShieldData* ShieldRow = InRow.GetRow<FShieldData>(ResolveContext);
		if (ShieldRow == nullptr)
		{
			return false;
		}

		OutViewData.GridSize = FIntPoint(ShieldRow->GridWidth, ShieldRow->GridHeight);
		OutViewData.EquipSlotType = ShieldRow->EquipSlotType;
		OutViewData.MaxStack = 1;
		OutViewData.DisplayName = ShieldRow->ShieldName;
		OutViewData.IconPath = ShieldRow->Icon ? ShieldRow->Icon->GetPathName() : FString();
		OutViewData.SellValue = ShieldRow->SellValue;
		OutViewData.Icon = ShieldRow->Icon;
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

UTexture2D* LoadItemIcon(const FString& InIconPath)
{
	if (InIconPath.IsEmpty())
	{
		return nullptr;
	}

	FString ObjectPath = InIconPath;
	FString PackagePath;
	FString AssetName;

	if (!ObjectPath.Split(TEXT("."), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		ObjectPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		ObjectPath = InIconPath + TEXT(".") + AssetName;
	}

	return LoadObject<UTexture2D>(nullptr, *ObjectPath);
}