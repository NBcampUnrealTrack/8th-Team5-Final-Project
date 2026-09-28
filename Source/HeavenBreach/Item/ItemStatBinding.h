#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "Item/ItemOptionTypes.h"

struct FGameplayEffectSpec;

// 퍼센트 옵션을 칸에 어떻게 붙이나.
enum class EItemPercentMode : uint8
{
	// 칸에 진짜 숫자가 있을 때. 칸 값에 곱한다.      공격력 53, +10% → 53 × 1.1
	MultiplyValue,

	// 칸에 배율이 있을 때. 배율에 더한다.            이동속도 배율 1.0, +10% → 1.1
	// 곱하지 않는 이유: 헤드샷 보너스처럼 0 에서 시작하는 칸은 곱하면 영원히 0 이다.
	AddToRatio,
};

// 스탯 이름표 하나가 어느 어트리뷰트 칸으로 가는지 적은 한 줄.
//
// 왜 표로 두나: GE 모디파이어를 만드는 곳, 값을 채우는 곳, 툴팁이 같은 목록을 봐야 한다.
// 셋이 이 표 하나를 돌면 한 곳만 고쳐서 어긋나는 일이 없다.
struct FItemStatBinding
{
	FGameplayTag Stat;
	FString DisplayName;                    // 툴팁 이름. "공격력"

	FGameplayAttribute FlatAttribute;       // + 가 가는 칸. 비면 + 를 안 받는다
	FGameplayTag FlatDataTag;

	FGameplayAttribute PercentAttribute;    // % 가 가는 칸. 비면 % 를 안 받는다
	FGameplayTag PercentDataTag;
	EItemPercentMode PercentMode = EItemPercentMode::AddToRatio;
};

// 스탯 하나의 합계. Percent 는 환산된 값이다 — DT 의 10 은 여기서 0.10.
struct FItemStatTotal
{
	float Flat = 0.f;
	float Percent = 0.f;
};

using FItemStatTotals = TMap<FGameplayTag, FItemStatTotal>;

namespace ItemStats
{
	const TArray<FItemStatBinding>& GetBindings();
	const FItemStatBinding* FindBinding(const FGameplayTag& Stat);
	FItemStatTotal GetTotal(const FItemStatTotals& Totals, const FGameplayTag& Stat);

	// % 합계를 GE 에 넘길 값으로.  MultiplyValue +10% → 1.1 / AddToRatio +10% → 0.1
	float MakePercentMagnitude(const FItemStatBinding& Binding, float PercentSum);

	// 옵션 한 줄을 합계에 더한다. DT 의 10 은 여기서 0.10 이 된다.
	// 표에 없는 스탯이거나 그 스탯이 그 타입(+/%)을 받지 않으면 false — 부르는 쪽이 경고를 찍는다.
	// 장비의 숫자 옵션과 아이템 동작 효과가 같이 쓴다. 그래서 "공격력 +10%" 의 뜻이 두 곳에서 같다.
	bool AddOptionToTotals(const FItemOption& Option, FItemStatTotals& Totals);

	// 합계를 GE 스펙의 SetByCaller 칸에 채운다. 표의 모든 칸을 채운다 — 안 채운 칸은 GAS 가 경고를 찍는다.
	// UGE_EquipmentStats 와 그 자식 UGE_ItemEffectStats 의 스펙에 쓴다.
	void FillSpec(FGameplayEffectSpec& Spec, const FItemStatTotals& Totals);

	// 툴팁 한 줄.  "공격력 +5", "방어력 -10%"
	FString FormatOption(const FItemOption& Option);

	// 툴팁 한 칸(초록 또는 빨강)에 들어갈 여러 줄. 숫자 옵션 먼저, 동작 효과 설명 다음.
	// 동작 효과는 정의(DT_ItemEffect)의 문구 틀에 아이템의 숫자를 끼워 만든다.
	FString FormatLines(const TArray<FItemOption>& Options, const TArray<FItemEffect>& Effects);
}
