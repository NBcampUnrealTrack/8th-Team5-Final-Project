// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HBBoxLoot.generated.h"

/**
 * 상자 보상 종류. 상자는 한 번 뽑을 때마다 종류를 먼저 고르고, 그 종류 안에서 등급·아이템을 고른다.
 * 일반 아이템 표(약 110줄)가 무기·방패 표보다 훨씬 커서, 종류 없이 고르면 재료·잡템이 대부분이 된다.
 */
UENUM(BlueprintType)
enum class EBoxLootCategory : uint8
{
	Equipment	UMETA(DisplayName = "장비"),		// 무기·방패 표 전부 + 방어구·장신구·가방·횃불
	Key			UMETA(DisplayName = "열쇠"),
	Consumable	UMETA(DisplayName = "소모품"),
	Other		UMETA(DisplayName = "재료·잡템"),	// 재료·잡템·설계도·화폐 등 나머지
};

namespace HBBoxLoot
{
	/** 일반 아이템 표의 ItemType 문자열을 종류로. 대소문자 무시, 모르는 종류는 Other */
	inline EBoxLootCategory CategoryOfItemType(const FString& ItemType)
	{
		static const TCHAR* EquipmentTypes[] = { TEXT("Helmet"), TEXT("Chest"), TEXT("Legs"), TEXT("Accessory"), TEXT("Bag"), TEXT("Torch") };
		for (const TCHAR* Type : EquipmentTypes)
		{
			if (ItemType.Equals(Type, ESearchCase::IgnoreCase))
			{
				return EBoxLootCategory::Equipment;
			}
		}
		if (ItemType.Equals(TEXT("Key"), ESearchCase::IgnoreCase))
		{
			return EBoxLootCategory::Key;
		}
		if (ItemType.Equals(TEXT("Consumable"), ESearchCase::IgnoreCase))
		{
			return EBoxLootCategory::Consumable;
		}
		return EBoxLootCategory::Other;
	}

	/** 상자 등급 표에 종류 비율이 비어 있으면 기본 비율(장비 40 / 열쇠 15 / 소모품 20 / 기타 25), 있으면 그대로 */
	inline TMap<EBoxLootCategory, float> ResolveCategoryWeights(const TMap<EBoxLootCategory, float>& FromTable)
	{
		if (FromTable.Num() > 0)
		{
			return FromTable;
		}
		TMap<EBoxLootCategory, float> Defaults;
		Defaults.Add(EBoxLootCategory::Equipment, 40.f);
		Defaults.Add(EBoxLootCategory::Key, 15.f);
		Defaults.Add(EBoxLootCategory::Consumable, 20.f);
		Defaults.Add(EBoxLootCategory::Other, 25.f);
		return Defaults;
	}

	/** Roll(0~1)로 가중치 하나를 고른다. 0 이하 가중치는 건너뛴다. 고를 게 없으면 INDEX_NONE */
	inline int32 PickWeightedIndex(const TArray<float>& Weights, float Roll01)
	{
		float Total = 0.f;
		int32 LastValid = INDEX_NONE;
		for (int32 i = 0; i < Weights.Num(); ++i)
		{
			if (Weights[i] > 0.f)
			{
				Total += Weights[i];
				LastValid = i;
			}
		}
		if (LastValid == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		float Remaining = FMath::Clamp(Roll01, 0.f, 1.f) * Total;
		for (int32 i = 0; i < Weights.Num(); ++i)
		{
			if (Weights[i] <= 0.f)
			{
				continue;
			}
			if (Remaining < Weights[i])
			{
				return i;
			}
			Remaining -= Weights[i];
		}
		return LastValid;
	}
}
