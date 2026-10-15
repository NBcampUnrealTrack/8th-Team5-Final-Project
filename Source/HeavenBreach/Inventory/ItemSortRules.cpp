#include "Inventory/ItemSortRules.h"
#include "Engine/DataTable.h"
#include "Item/Item.h"

namespace HBItemSort
{
	namespace
	{
		int32 GetWeaponTypeRank(const FString& InWeaponType)
		{
			if (InWeaponType == TEXT("Sword")) return 0;
			if (InWeaponType == TEXT("Dagger")) return 1;
			if (InWeaponType == TEXT("Bow")) return 2;
			if (InWeaponType == TEXT("Staff")) return 3;
			return 4;
		}

		int32 GetBlueprintRank(const FName InRowName)
		{
			FString Prefix = InRowName.ToString();
			int32 UnderscoreIndex = INDEX_NONE;
			if (Prefix.FindChar(TEXT('_'), UnderscoreIndex))
			{
				Prefix.LeftInline(UnderscoreIndex);
			}

			static const TCHAR* Order[] = { TEXT("BPSW"), TEXT("BPD"), TEXT("BPB"), TEXT("BPST"), TEXT("BPS"), TEXT("BPH"), TEXT("BPC"), TEXT("BPL") };
			const int32 OrderCount = static_cast<int32>(UE_ARRAY_COUNT(Order));
			for (int32 Index = 0; Index < OrderCount; ++Index)
			{
				if (Prefix == Order[Index])
				{
					return Index;
				}
			}
			return OrderCount;
		}
	}

	int32 GetCategoryRank(const FDataTableRowHandle& InItemRow, const FItemViewData& InViewData)
	{
		const UDataTable* Table = InItemRow.DataTable;
		const UScriptStruct* RowStruct = Table ? Table->GetRowStruct() : nullptr;

		if (RowStruct && RowStruct->IsChildOf(FWeaponData::StaticStruct()))
		{
			const FWeaponData* WeaponRow = InItemRow.GetRow<FWeaponData>(TEXT("ItemSort"));
			return WeaponRow ? GetWeaponTypeRank(WeaponRow->WeaponType) : 4;
		}

		if (RowStruct && RowStruct->IsChildOf(FShieldData::StaticStruct()))
		{
			return 10;
		}

		if (InViewData.ItemType == TEXT("Currency")) return -10;
		if (InViewData.ItemType == TEXT("Blueprint")) return 60 + GetBlueprintRank(InItemRow.RowName);

		switch (InViewData.EquipSlotType)
		{
		case EEquipSlotType::PrimaryWeapon:		return 4;
		case EEquipSlotType::SecondaryEquip:	return 11;
		case EEquipSlotType::Helmet:			return 20;
		case EEquipSlotType::Chest:				return 21;
		case EEquipSlotType::Legs:				return 22;
		case EEquipSlotType::Accessory:			return 25;
		case EEquipSlotType::Backpack:			return 30;
		default: break;
		}

		if (InViewData.ItemType == TEXT("Bag")) return 30;
		if (InViewData.ItemType == TEXT("Consumable"))
		{
			const bool bPotion = InViewData.HealAmount > 0.f || InViewData.ManaAmount > 0.f || InViewData.BuffDuration > 0.f || InViewData.FallDamageReduction > 0.f;
			return bPotion ? 40 : 42;
		}
		if (InViewData.ItemType == TEXT("Arrow")) return 41;
		if (InViewData.ItemType == TEXT("Key")) return 45;
		if (InViewData.ItemType == TEXT("Material")) return 50;
		if (InViewData.ItemType == TEXT("Junk")) return 55;
		return 90;
	}
}
