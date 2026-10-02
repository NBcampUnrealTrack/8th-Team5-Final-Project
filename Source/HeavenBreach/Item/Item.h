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
	// 옛 AttackRange(사거리) 칸은 지웠다. 던전 기본 공격(GA_PlayerAttackRE)은 무기 휘두르기 판정이라
	// 이 값을 안 읽었다. 읽던 곳은 테스트 맵의 옛 공격 GA 와 부여되지 않는 전사 액티브뿐이었다.
	// 공격 속도 배율. 1 = 기본, 0.8 = 20% 느림, 1.2 = 20% 빠름.
	// 각인 · 악세서리의 공격 속도 % 와 같은 칸에 더해진다 — 0.8 은 -20% 로 들어간다.
	//   전사 1.0 + 검 0.8 = 0.8 / 궁수 1.2 + 검 0.8 = 1.0 (곱이 아니라 합)
	// 왜 바꿨나: 옛 칸은 "공격 간격(초)"(AttackInterval)이었는데 읽는 곳이 툴팁뿐이라 실제로는 반영되지 않았다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float AttackSpeedRate = 1.f;
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

	// 몬스터/상자 드랍 풀 포함 여부. 에셋 미완성 행은 꺼두면 자동 제외된다(다른 획득 경로엔 영향 없음).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bDroppable = true;

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
	// 옛 DamageReduction(피해 감소 %) 칸은 지웠다. 막기는 각도 안이면 100% 무효라 읽는 곳이 툴팁뿐이었다.
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

	// 몬스터/상자 드랍 풀 포함 여부. 에셋 미완성 행은 꺼두면 자동 제외된다(다른 획득 경로엔 영향 없음).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bDroppable = true;

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

	// 몬스터/상자 드랍 풀 포함 여부. 에셋 미완성 아이템(설계도, 열쇠 등)은 꺼두면
	// 자동 제외된다 - 상점 판매/제작 결과물 등 다른 획득 경로엔 영향 없음, 드랍 풀에서만 뺀다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bDroppable = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString UseEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WorldMeshPath;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (ClampMin = "0.01"))
	float WorldScaleMultiplier = 1.f;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ManaAmount = 0.f;

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