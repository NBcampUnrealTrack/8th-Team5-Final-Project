// Copyright RelicsOfTheFallenHeaven. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Item/ItemOptionTypes.h"
#include "Item/ItemEngravingTypes.h"
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
	Backpack		UMETA(DisplayName = "Backpack"),
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
	FString WorldMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FString ItemType;
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 SellValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	bool IsStackable() const { return MaxStack > 1; }
	bool IsEquippable() const { return EquipSlotType != EEquipSlotType::None; }
	UPROPERTY(BlueprintReadOnly)
	float HealAmount = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float ManaAmount = 0.f;
	bool IsConsumable() const { return HealAmount > 0.f || ManaAmount > 0.f; }

	UPROPERTY(BlueprintReadOnly, Category = "Bag")
	FIntPoint BagGridSize = FIntPoint::ZeroValue;

	bool IsBag() const { return BagGridSize.X > 0 && BagGridSize.Y > 0; }

	// ===== 악세서리 옵션 =====
	// FItemData 의 같은 이름 칸을 그대로 옮겨 담는다. 무기·방패 행은 비어서 온다.
	// 옛 Relic 칸(bIsRelic · Benefit/RiskDescription · Benefit/RiskEffect)은 유물 통합으로 지웠다.

	UPROPERTY(BlueprintReadOnly, Category = "Option")
	TArray<FItemOption> BenefitOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Option")
	TArray<FItemOption> RiskOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Option")
	TArray<FItemEffect> BenefitEffects;

	UPROPERTY(BlueprintReadOnly, Category = "Option")
	TArray<FItemEffect> RiskEffects;
};

USTRUCT(BlueprintType)
struct FBagItemEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FDataTableRowHandle ItemRow;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid OriginalOwnerId;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bRotated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bIsFavorite = false;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float CurrentDurability = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FItemEngraving Engraving;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FIntPoint Position = FIntPoint::ZeroValue;
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

	// 상자/몬스터 시체 같은 루팅 컨테이너에서 아직 "서칭"되지 않은 아이템인지.
	// 기본값 true(공개된 상태) - 플레이어 인벤토리로 들어오는 일반 아이템(픽업/제작/구매 등)은
	// 전부 이 기본값을 그대로 쓴다. 루팅 컨테이너가 아이템을 생성할 때(BoxActor::GenerateLoot,
	// ULootComponent::AddLootItem)만 명시적으로 false로 만들어서, 컨테이너 화면이 열려 있는 동안
	// UInventoryComponent::BeginItemRevealSequence()가 하나씩 순서대로 true로 뒤집어준다.
	// false인 동안에는 UI가 "?"로 표시하고(InventoryItemWidget::InitializeItem), 다른 인벤토리로의
	// 이동/병합도 막는다(TransferItemFrom/TransferItemFromAuto/MergeStackFrom) - 아직 뭔지 모르는
	// 아이템을 미리 가져갈 수 없게 하기 위함.
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bRevealed = true;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float CurrentDurability = -1.f;

	// 랜덤 각인. 장비(방어구 · 무기 · 방패)만 받는다. 규칙은 Item/ItemEngraving.
	// 복제 · 창고 이동 · 저장 · 버리기/줍기로 통째로 따라다닌다.
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FItemEngraving Engraving;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TArray<FBagItemEntry> BagContents;

	bool IsBroken() const { return CurrentDurability == 0.f; }

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

HEAVENBREACH_API FInventoryEntry BagItemToInventoryEntry(const FBagItemEntry& InBagItem);
HEAVENBREACH_API FBagItemEntry InventoryEntryToBagItem(const FInventoryEntry& InEntry);

HEAVENBREACH_API FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData);

HEAVENBREACH_API float GetMaxDurabilityForGrade(EItemGrade InGrade);

HEAVENBREACH_API FLinearColor GetGradeTopColor(EItemGrade InGrade);
HEAVENBREACH_API FLinearColor GetGradeBottomColor(EItemGrade InGrade);

// 인벤토리 칸 아이콘 배경 전용 색상. GetGradeTopColor/BottomColor(위)는 그대로 두고,
// 아이템 정보 팝업 카드(ItemInfoPopupWidget)와 톤을 맞추기 위해 따로 정의한다 —
// 팝업처럼 "거의 검은 다크 베이스 + 등급색은 은은한 틴트"로 가져가서 카드형 UI와
// 통일감 있게 보이게 한다. Top/Mid/Bottom 3단은 목업 CSS의 0%/30%/70% 스탑에 대응하며,
// M_ItemGradeGradient의 동명 파라미터로 전달된다.
HEAVENBREACH_API FLinearColor GetInventoryIconTopColor(EItemGrade InGrade);
HEAVENBREACH_API FLinearColor GetInventoryIconBottomColor(EItemGrade InGrade);
HEAVENBREACH_API FLinearColor GetInventoryIconMidColor(EItemGrade InGrade);

HEAVENBREACH_API UTexture2D* LoadItemIcon(const FString& InIconPath);

HEAVENBREACH_API FString GetEquipSlotDisplayName(EEquipSlotType InSlot);

// 아이템의 스탯 요약 텍스트(무기 공격력/사거리/공속, 방패 피해감소/방어범위, 일반 아이템
// BonusStat 등)를 만들어서 정보 팝업에 그대로 넣을 수 있게 한다. 원래 UInventoryWidget
// 전용 private 멤버였는데, EquipmentSlotWidget(장착 슬롯 호버)에서도 동일한 팝업을 띄우려면
// 같이 써야 해서 공용 함수로 뺐다.
HEAVENBREACH_API FString BuildStatTextForItem(const FItemInstance& InItem);

// 툴팁 초록 / 빨강 칸 문구. DT 의 옵션 · 동작 효과에 이 아이템에 발린 각인을 더한다.
// 인벤토리 칸 호버와 장착 슬롯 호버가 같이 쓴다 — 한쪽 툴팁에만 각인이 보이는 일이 없게.
// BuildStatTextForItem 과 같은 이유로 공용 함수다.
HEAVENBREACH_API FString BuildBenefitLinesForItem(const FItemInstance& InItem, const FItemViewData& InViewData);
HEAVENBREACH_API FString BuildRiskLinesForItem(const FItemInstance& InItem, const FItemViewData& InViewData);