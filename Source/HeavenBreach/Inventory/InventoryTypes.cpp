#include "InventoryTypes.h"
#include "Item/Item.h"
#include "Engine/Texture2D.h"
#include "AbilitySystemComponent.h"


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
		OutViewData.Grade = ItemRow->Grade;
		OutViewData.DisplayName = ItemRow->ItemName;
		OutViewData.IconPath = ItemRow->Icon ? ItemRow->Icon->GetPathName() : FString();
		OutViewData.HealAmount = ItemRow->HealAmount;
		OutViewData.SellValue = ItemRow->SellValue;
		OutViewData.Icon = ItemRow->Icon;
		OutViewData.bIsRelic = ItemRow->bIsRelic;
		OutViewData.BenefitDescription = ItemRow->BenefitDescription;
		OutViewData.RiskDescription = ItemRow->RiskDescription;
		OutViewData.BenefitEffect = ItemRow->BenefitEffect;
		OutViewData.RiskEffect = ItemRow->RiskEffect;
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
		OutViewData.Grade = WeaponRow->Grade;
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
		OutViewData.Grade = ShieldRow->Grade;
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

float GetMaxDurabilityForGrade(EItemGrade InGrade)
{
	// 기획서 10.2 기준: 일반 100 / 희귀 150 / 영웅 200 / 천상(Legend) 250.
	// Relic/Ancient는 현재 유물 전용 등급이라 별도 규칙이 정해지기 전까지는
	// 천상과 동일하게 처리한다.
	switch (InGrade)
	{
	case EItemGrade::Normal:
		return 100.f;
	case EItemGrade::Rare:
		return 150.f;
	case EItemGrade::Epic:
		return 200.f;
	case EItemGrade::Legend:
	case EItemGrade::Relic:
	case EItemGrade::Ancient:
	default:
		return 250.f;
	}
}

FLinearColor GetGradeTopColor(EItemGrade InGrade)
{
	switch (InGrade)
	{
	case EItemGrade::Normal:
		return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
	case EItemGrade::Rare:
		return FLinearColor(0.4f, 0.62f, 1.0f, 1.0f);
	case EItemGrade::Epic:
		return FLinearColor(0.65f, 0.25f, 0.9f, 1.0f);
	case EItemGrade::Legend:
		return FLinearColor(1.0f, 0.78f, 0.2f, 1.0f);
	case EItemGrade::Relic:
		return FLinearColor(1.0f, 0.5f, 0.08f, 1.0f);
	case EItemGrade::Ancient:
		return FLinearColor(1.0f, 0.98f, 0.9f, 1.0f);
	default:
		return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
	}
}

FLinearColor GetGradeBottomColor(EItemGrade InGrade)
{
	switch (InGrade)
	{
	case EItemGrade::Normal:
		return FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
	case EItemGrade::Rare:
		return FLinearColor(0.08f, 0.14f, 0.35f, 1.0f);
	case EItemGrade::Epic:
		return FLinearColor(0.18f, 0.05f, 0.3f, 1.0f);
	case EItemGrade::Legend:
		return FLinearColor(0.42f, 0.26f, 0.04f, 1.0f);
	case EItemGrade::Relic:
		return FLinearColor(0.42f, 0.16f, 0.02f, 1.0f);
	case EItemGrade::Ancient:
		return FLinearColor(0.7f, 0.62f, 0.36f, 1.0f);
	default:
		return FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
	}
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