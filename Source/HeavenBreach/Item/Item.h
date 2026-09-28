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

	// 아이템 정보 팝업(상세 정보)에 표시되는 플레이버 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString Description;
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

	// 아이템 정보 팝업(상세 정보)에 표시되는 플레이버 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString Description;
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

	// 아이템 정보 팝업(상세 정보)에 표시되는 플레이버 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ShoesMeshPath;

	// 반대쪽 슬롯(Chest<->Legs)이 비어있을 때 기본 바디 대신 사용할 보정용 바디 메시 경로.
	// 예) Celestial 하의를 착용했는데 Chest가 비어있으면, 이 하의와 자연스럽게 어울리는
    // 상체 메시로 교체한다. Chest 아이템이면 이 필드는 "하체" 보정 메시를,
    // Legs 아이템이면 "상체" 보정 메시를 가리킨다. 비어있으면 기존처럼 기본 바디 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString PairedBodyMeshPath;

	// ===== 악세서리 옵션 =====
	// 좋은 쪽과 나쁜 쪽을 나눠 적는다. 툴팁 색(초록/빨강)이 이걸로 정해진다.
	// 부호로는 못 가른다 — "받는 피해 +20%" 는 + 인데 나쁜 옵션이다.
	// 계산은 네 배열을 적힌 부호 그대로 더할 뿐이다. 나뉜 건 표시뿐이다.
	// 숫자 옵션은 툴팁 설명이 자동으로 만들어진다.
	// 동작 효과는 DT_ItemEffect 의 정의를 고르고 숫자만 준다. 문구는 정의의 문구 틀에서 나온다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (TitleProperty = "Stat"))
	TArray<FItemOption> BenefitOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (TitleProperty = "Stat"))
	TArray<FItemOption> RiskOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (TitleProperty = "Definition"))
	TArray<FItemEffect> BenefitEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (TitleProperty = "Definition"))
	TArray<FItemEffect> RiskEffects;
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