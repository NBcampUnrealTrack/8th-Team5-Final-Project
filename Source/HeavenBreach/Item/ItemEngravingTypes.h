#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Item/ItemOptionTypes.h"
#include "ItemEngravingTypes.generated.h"

// 왜 따로 된 헤더인가: FItemInstance(InventoryTypes.h)와 AItemBase(ItemBase.h)가 둘 다
// 이 타입을 칸으로 가진다. ItemOptionTypes.h 와 같은 이유다.

// 아이템 하나에 발린 각인. 아이템(FItemInstance) · 땅바닥 액터(AItemBase) · 저장 JSON 이 이걸 통째로 옮긴다.
// 왜 한 구조체인가: 칸을 따로 두면 옮기는 곳마다 하나씩 빠뜨린다. 칸이 늘어도 여기만 고친다.
USTRUCT(BlueprintType)
struct FItemEngraving
{
	GENERATED_BODY()

	// 툴팁 초록 칸. 악세서리 숫자 옵션과 같은 타입이라 스탯 합산 · 툴팁이 그대로 읽는다.
	UPROPERTY(BlueprintReadOnly, Category = "Engraving")
	TArray<FItemOption> Benefits;

	// 툴팁 빨강 칸
	UPROPERTY(BlueprintReadOnly, Category = "Engraving")
	TArray<FItemOption> Risks;

	// 각인을 발랐는가. 굴린 결과가 0개여도 true 다.
	// 왜 따로 두나: 옵션이 비어 있는 것만으로는 "0개가 나왔음" 과 "아직 안 굴렸음" 을 가를 수 없다.
	// false 면 다음에 인벤토리에 들어올 때 · 불러올 때 굴린다. 등급 행이 없는 아이템(일반 등)은 false 로 남는다.
	UPROPERTY(BlueprintReadOnly, Category = "Engraving")
	bool bEngraved = false;
};

// 각인으로 뽑힐 수 있는 옵션 한 줄. DT_Engrave 의 등급 행 안에 목록으로 들어간다.
USTRUCT(BlueprintType)
struct FEngraveOptionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemOptionValueType ValueType = EItemOptionValueType::Flat;

	// 굴릴 범위. 적힌 부호 그대로 쓴다 — 나쁜 옵션은 보통 음수 (받는 피해처럼 + 가 나쁜 스탯은 양수).
	// Min 과 Max 를 거꾸로 적어도 된다 (-2 ~ -3 과 -3 ~ -2 는 같다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Max = 0.f;

	// 굴린 값을 이 단위로 맞춘다. 1 = 정수, 0.5, 0.1 …
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float Step = 1.f;

	// 뽑힐 가중치. 1 이 기본(균등). 0 이면 안 뽑힌다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// 붙는 부위. 지금은 셋 다 켜 둔다.
	// 부위마다 다르게 하려면 끄거나, 같은 스탯을 두 줄로 두고 부위를 나눈다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	bool bWeapon = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	bool bShield = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	bool bArmor = true;
};

// DT_Engrave 의 한 행 = 한 등급. 행 이름을 등급 이름(Rare / Epic / Legend)으로 둔다.
// 행이 없는 등급(Normal 등)은 각인을 안 받는다. 나중에 행을 만들면 가진 아이템도 다음에 불러올 때 발린다.
USTRUCT(BlueprintType)
struct FEngraveGradeRow : public FTableRowBase
{
	GENERATED_BODY()

	// 좋은 옵션 개수. min~max 사이에서 뽑는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count", meta = (ClampMin = "0"))
	int32 BenefitCountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count", meta = (ClampMin = "0"))
	int32 BenefitCountMax = 0;

	// 개수별 가중치. [0] 이 min 개, [1] 이 min+1 개 … 비우면 균등.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count")
	TArray<float> BenefitCountWeights;

	// 나쁜 옵션 개수. 좋은 옵션과 따로 뽑는다 (독립 시행). 기본 0~1 = 50%.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count", meta = (ClampMin = "0"))
	int32 RiskCountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count", meta = (ClampMin = "0"))
	int32 RiskCountMax = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Count")
	TArray<float> RiskCountWeights;

	// 이 등급에서 뽑힐 수 있는 옵션들
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool", meta = (TitleProperty = "Stat"))
	TArray<FEngraveOptionEntry> Benefits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool", meta = (TitleProperty = "Stat"))
	TArray<FEngraveOptionEntry> Risks;
};
