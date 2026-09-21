// Copyright RelicsOfTheFallenHeaven. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
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
	bool IsConsumable() const { return HealAmount > 0.f; }

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	bool bIsRelic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	FString BenefitDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	FString RiskDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> BenefitEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Relic")
	TSubclassOf<UGameplayEffect> RiskEffect;
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

	// ��� ������. -1�̸� "�� �ν��Ͻ��� ���� �������� �ʱ�ȭ���� ����"�� �ǹ��ϰ�,
	// ����/�Ĺ����� �������� ó�� �����Ǵ� ������ ��޺� �ִ�ġ�� ä����� �Ѵ�.
	// 0�� �Ǹ� �ļ� ����(����/���� ���/�簢�� �Ұ�, ����/�Ǹ�/���ظ� ����).
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float CurrentDurability = -1.f;

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

HEAVENBREACH_API FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData);

// ��޺� �ִ� ������(��ȹ�� 10.2 ����: �Ϲ� 100 / ��� 150 / ���� 200 / õ�� 250).
// �뷱�� ������ ���� �� ���� ��ġ�� �ȴ�.
HEAVENBREACH_API float GetMaxDurabilityForGrade(EItemGrade InGrade);

// ��޺� ��� �׶��̼� ����(�κ��丮 ĭ/���� ���� ����). �� ������ ���� �� ����
// ��ġ�� InventoryItemWidget, EquipmentSlotWidget ���ʿ� �� �ݿ��ȴ�.
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