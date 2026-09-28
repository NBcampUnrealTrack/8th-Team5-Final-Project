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
		OutViewData.ItemType = ItemRow->ItemType;
		OutViewData.IconPath = ItemRow->Icon ? ItemRow->Icon->GetPathName() : FString();
		OutViewData.HealAmount = ItemRow->HealAmount;
		OutViewData.SellValue = ItemRow->SellValue;
		OutViewData.Description = ItemRow->Description;
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
		OutViewData.Description = WeaponRow->Description;
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
		OutViewData.Description = ShieldRow->Description;
		OutViewData.Icon = ShieldRow->Icon;
		return true;
	}

	return false;
}

FString GetEquipSlotDisplayName(EEquipSlotType InSlot)
{
	switch (InSlot)
	{
	case EEquipSlotType::PrimaryWeapon:  return TEXT("주무기");
	case EEquipSlotType::SecondaryEquip: return TEXT("보조 장비");
	case EEquipSlotType::Helmet:         return TEXT("머리");
	case EEquipSlotType::Chest:          return TEXT("상의");
	case EEquipSlotType::Legs:           return TEXT("하의");
	case EEquipSlotType::Accessory:      return TEXT("장신구");
	case EEquipSlotType::None:
	default:                             return FString();
	}
}

FString BuildStatTextForItem(const FItemInstance& InItem)
{
	FString StatText;
	const UDataTable* ItemDataTable = InItem.ItemRow.DataTable;
	if (ItemDataTable == nullptr)
	{
		return StatText;
	}

	const UScriptStruct* RowStruct = ItemDataTable->GetRowStruct();
	if (RowStruct == FWeaponData::StaticStruct())
	{
		if (const FWeaponData* WeaponRow = ItemDataTable->FindRow<FWeaponData>(InItem.ItemRow.RowName, TEXT("ItemInfoHover")))
		{
			const FText WeaponStatFormat = FText::FromString(TEXT("공격력: {0}\n사거리: {1}\n공격 속도: {2}초"));
			StatText = FText::Format(WeaponStatFormat,
				FText::AsNumber(WeaponRow->AttackPower),
				FText::AsNumber(WeaponRow->AttackRange),
				FText::AsNumber(WeaponRow->AttackInterval)).ToString();
		}
	}
	else if (RowStruct == FShieldData::StaticStruct())
	{
		if (const FShieldData* ShieldRow = ItemDataTable->FindRow<FShieldData>(InItem.ItemRow.RowName, TEXT("ItemInfoHover")))
		{
			const FText ShieldStatFormat = FText::FromString(TEXT("피해 감소: {0}%\n방어 범위: {1}도"));
			StatText = FText::Format(ShieldStatFormat,
				FText::AsNumber(ShieldRow->DamageReduction),
				FText::AsNumber(ShieldRow->BlockAngle)).ToString();
		}
	}
	else if (RowStruct == FItemData::StaticStruct())
	{
		if (const FItemData* GeneralRow = ItemDataTable->FindRow<FItemData>(InItem.ItemRow.RowName, TEXT("ItemInfoHover")))
		{
			if (!GeneralRow->BonusStatType.IsEmpty())
			{
				const FText BonusStatFormat = FText::FromString(TEXT("{0}: {1}"));
				StatText = FText::Format(BonusStatFormat,
					FText::FromString(GeneralRow->BonusStatType),
					FText::AsNumber(GeneralRow->BonusStatValue)).ToString();
			}
		}
	}
	return StatText;
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

namespace
{
	// ItemInfoPopupWidget.cpp의 다크 카드 배경과 같은 베이스 톤(#171411). 인벤토리 아이콘
	// 배경도 이 베이스에서 시작해서 팝업과 같은 "다크 카드 + 등급색 은은한 틴트" 느낌으로 맞춘다.
	FLinearColor GetInventoryIconBaseColor()
	{
		return FLinearColor(FColor(0x17, 0x14, 0x11, 255));
	}
}

FLinearColor GetInventoryIconTopColor(EItemGrade InGrade)
{
	// 목업(대각선 샤인 참고 디자인)에서 확인된 실제 sRGB 색을 FColor 헥스로 그대로
	// 넣고 FLinearColor로 변환(정확한 sRGB->리니어)한다. 등급별 색상 계열은
	// GetGradeTopColor와 동일하되 채도/밝기는 목업 기준으로 맞췄다.
	switch (InGrade)
	{
	case EItemGrade::Normal:
		return FLinearColor(FColor(0x9a, 0x9a, 0x9a, 255));
	case EItemGrade::Rare:
		return FLinearColor(FColor(0x3d, 0x6b, 0xd6, 255));
	case EItemGrade::Epic:
		return FLinearColor(FColor(0x7a, 0x3a, 0xa8, 255));
	case EItemGrade::Legend:
		return FLinearColor(FColor(0xe0, 0xa6, 0x3a, 255));
	case EItemGrade::Relic:
		return FLinearColor(FColor(0xc9, 0x50, 0x0f, 255));
	case EItemGrade::Ancient:
		return FLinearColor(FColor(0xf2, 0xe6, 0xc9, 255));
	default:
		return FLinearColor(FColor(0x9a, 0x9a, 0x9a, 255));
	}
}

FLinearColor GetInventoryIconMidColor(EItemGrade InGrade)
{
	// 목업 CSS의 30% 지점 색. Epic(#2c1338)/Legend(#4a3313)는 목업 실측값 그대로,
	// 나머지 등급은 베이스에서 등급색 쪽으로 약 22% 틴트해서 계산했다.
	switch (InGrade)
	{
	case EItemGrade::Normal:
		return FLinearColor(FColor(0x34, 0x31, 0x2f, 255));
	case EItemGrade::Rare:
		return FLinearColor(FColor(0x1f, 0x27, 0x3c, 255));
	case EItemGrade::Epic:
		return FLinearColor(FColor(0x2c, 0x13, 0x38, 255));
	case EItemGrade::Legend:
		return FLinearColor(FColor(0x4a, 0x33, 0x13, 255));
	case EItemGrade::Relic:
		return FLinearColor(FColor(0x3e, 0x21, 0x11, 255));
	case EItemGrade::Ancient:
		return FLinearColor(FColor(0x47, 0x42, 0x39, 255));
	default:
		return FLinearColor(FColor(0x34, 0x31, 0x2f, 255));
	}
}

FLinearColor GetInventoryIconBottomColor(EItemGrade InGrade)
{
	// 목업의 대각선 반대쪽 끝(70~100% 지점)도 카드 베이스 톤(#171411) 그대로라,
	// 등급별로 다르게 섞을 필요 없이 베이스 컬러를 그대로 반환한다.
	(void)InGrade;
	return GetInventoryIconBaseColor();
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