#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "ItemOptionTypes.generated.h"

// 왜 따로 된 헤더인가: FItemData(Item.h)와 FItemViewData(InventoryTypes.h)가 둘 다
// 이 타입을 칸으로 가진다. Item.h 가 InventoryTypes.h 를 포함하고 있어서, 여기 말고
// Item.h 에 두면 InventoryTypes.h 가 쓸 수 없다.

// 옵션 값이 수치인지 퍼센트인지.
UENUM(BlueprintType)
enum class EItemOptionValueType : uint8
{
	Flat     UMETA(DisplayName = "+"),
	Percent  UMETA(DisplayName = "%"),
};

// 숫자 옵션 한 줄.
//
// 값은 적힌 그대로 그 스탯 칸에 들어간다. 코드가 부호를 바꾸지 않는다.
//   Stat.AttackPower  +   5     공격력 5 오름
//   Stat.Defense      %  -10    방어력 10% 내림
//   Stat.Cooldown     %  -10    쿨다운 10% 짧아짐
// Percent 는 10 이라고 적으면 10% 다.
//
// 왜 태그인가: BonusStatType 이 문자열이라 "방어력"/"DefenseRate" 표기 차이로
// 방어구 방어력이 오래 0 이었다. 태그 드롭다운이면 오타가 날 수 없다.
USTRUCT(BlueprintType)
struct FItemOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemOptionValueType ValueType = EItemOptionValueType::Flat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Value = 0.f;
};

// ================= 동작 효과 =================
//
// 숫자 옵션으로 표현이 안 되는 효과 ("주변 파티원 방어력 증가", "스킬 쓸 때 체력 소모" 같은 것).
// "무엇을" 과 "얼마나" 를 나눈다 — 스카이림의 마법 효과와 같은 나눔.
//   무엇을  = 효과 정의 (FItemEffectDef). DT_ItemEffect 한 곳에 모인다.
//   얼마나  = 아이템의 효과 줄 (FItemEffect). 정의를 고르고 숫자만 준다.
// 코드는 효과의 "종류"(원형) 5가지만 안다. 규칙은 Item/ItemEffectRules 에 있다.

// 효과의 종류. 새 효과가 이 중 하나로 표현되면 DT 한 줄로 끝난다.
// 새 종류가 필요하면 여기에 하나 더하고 ItemEffectRules 에 처리를 더한다.
UENUM(BlueprintType)
enum class EItemEffectArchetype : uint8
{
	Display         UMETA(DisplayName = "표시만 (태그)"),
	Aura            UMETA(DisplayName = "오라: 파티원 스탯"),
	WhileCondition  UMETA(DisplayName = "조건 동안: 내 스탯"),
	Periodic        UMETA(DisplayName = "주기: 체력 증감"),
	OnSkillUse      UMETA(DisplayName = "스킬 사용 시: 체력 증감"),
};

// 효과가 켜지는 조건. 효과 줄의 조건값이 무슨 뜻인지가 여기서 정해진다.
UENUM(BlueprintType)
enum class EItemEffectCondition : uint8
{
	Always       UMETA(DisplayName = "항상"),
	HealthBelow  UMETA(DisplayName = "체력이 조건값 % 이하"),
	ManaFull     UMETA(DisplayName = "마나 가득"),
};

// 효과 정의. DT_ItemEffect 의 한 줄. 효과는 전부 여기 모인다.
// 아이템은 이 줄을 고르고 숫자(효과량 · 조건값)만 준다.
USTRUCT(BlueprintType)
struct FItemEffectDef : public FTableRowBase
{
	GENERATED_BODY()

	// 끼운 사람에게 붙는 표시. 에디터 태그 관리자에서 새로 만들어도 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Item.Effect"))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemEffectArchetype Archetype = EItemEffectArchetype::Display;

	// 조건 동안 · 주기 · 스킬 사용 시 가 본다. 오라는 안 본다 (오라의 조건값은 반경이다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemEffectCondition ConditionType = EItemEffectCondition::Always;

	// 오라 · 조건 동안 이 바꾸는 스탯. 숫자 옵션과 같은 스탯 표를 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemOptionValueType ValueType = EItemOptionValueType::Flat;

	// 주기 효과의 간격(초). 주기 가 아니면 안 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float Interval = 1.f;

	// 효과를 받는 쪽에 붙는 이름표. 상태 UI 가 찾는다. 비워도 된다.
	// Item.Effect.* 금지 — 받은 사람이 끼운 사람으로 보여 오라가 옮겨 붙는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag ReceiverTag;

	// 툴팁 문구 틀. {Amount} {Condition} 이 아이템의 숫자로 바뀐다.
	//   예) "파티원 방어력 +{Amount}"  →  "파티원 방어력 +10"
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Description;
};

// 아이템에 붙은 효과 한 줄 (DT_Item 의 BenefitEffects / RiskEffects).
// 무엇을 할지는 정의가, 얼마나 할지는 여기 숫자가 정한다.
USTRUCT(BlueprintType)
struct FItemEffect
{
	GENERATED_BODY()

	// DT_ItemEffect 의 한 줄
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDataTableRowHandle Definition;

	// 얼마나. 오라 · 조건 동안 = 스탯 값 / 주기 = 1회 체력 변화 / 스킬 사용 시 = 최대 체력 %
	// 체력은 - 가 피해, + 가 회복. 적힌 부호 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "효과량"))
	float Amount = 0.f;

	// 조건의 기준. 오라 = 반경 / 체력 이하 = % / 그 밖 = 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "조건값"))
	float Condition = 0.f;
};
