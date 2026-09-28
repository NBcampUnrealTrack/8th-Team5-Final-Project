#include "Item/ItemStatBinding.h"

#include "GameplayEffect.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Item/ItemEffectRules.h"

using namespace ItemTag;
using Attr = UHB_PlayerAttributeSet;

namespace
{
	// 툴팁 한 칸에 줄을 붙인다. 첫 줄 앞에는 줄바꿈을 넣지 않는다.
	void AppendLine(FString& Text, const FString& Line)
	{
		if (!Text.IsEmpty())
		{
			Text += TEXT("\n");
		}
		Text += Line;
	}

	// 표에 한 줄 넣는다. 매번 새 구조체라 앞 줄 값이 남지 않는다.
	void AddBinding(TArray<FItemStatBinding>& Out,
		const FGameplayTag& Stat, const TCHAR* DisplayName,
		const FGameplayAttribute& FlatAttribute, const FGameplayTag& FlatDataTag,
		const FGameplayAttribute& PercentAttribute, const FGameplayTag& PercentDataTag,
		EItemPercentMode PercentMode)
	{
		FItemStatBinding B;
		B.Stat             = Stat;
		B.DisplayName      = DisplayName;
		B.FlatAttribute    = FlatAttribute;
		B.FlatDataTag      = FlatDataTag;
		B.PercentAttribute = PercentAttribute;
		B.PercentDataTag   = PercentDataTag;
		B.PercentMode      = PercentMode;
		Out.Add(B);
	}

	TArray<FItemStatBinding> BuildBindings()
	{
		// "이 칸은 안 받는다" 표시. 빈 어트리뷰트는 IsValid() 가 false 라 GE 모디파이어도 안 생긴다.
		const FGameplayAttribute NoAttribute;
		const FGameplayTag NoTag;
		const EItemPercentMode Multiply = EItemPercentMode::MultiplyValue;
		const EItemPercentMode Add = EItemPercentMode::AddToRatio;

		TArray<FItemStatBinding> B;

		// ===== 칸에 진짜 숫자 — + 는 더하고 % 는 곱한다. 둘 다 같은 칸 =====

		// 공격력 = (직업 + 무기 + 옵션+) × (1 + 옵션%)
		AddBinding(B, TAG_Stat_AttackPower, TEXT("공격력"),
			Attr::GetPlayer_AttackPowerAttribute(), TAG_Data_Equipment_AttackPower_Flat,
			Attr::GetPlayer_AttackPowerAttribute(), TAG_Data_Equipment_AttackPower_Percent, Multiply);

		// 방어력 = (직업 + 방어구 + 옵션+) × (1 + 옵션%)
		AddBinding(B, TAG_Stat_Defense, TEXT("방어력"),
			Attr::GetPlayer_DefenseAttribute(), TAG_Data_Equipment_Defense_Flat,
			Attr::GetPlayer_DefenseAttribute(), TAG_Data_Equipment_Defense_Percent, Multiply);

		// 최대 체력 = (직업 + 옵션+) × (1 + 옵션%)
		AddBinding(B, TAG_Stat_MaxHP, TEXT("최대 체력"),
			Attr::GetPlayer_MaxHPAttribute(), TAG_Data_Equipment_MaxHP_Flat,
			Attr::GetPlayer_MaxHPAttribute(), TAG_Data_Equipment_MaxHP_Percent, Multiply);

		// 최대 스태미나 = (직업 + 옵션+) × (1 + 옵션%). 마나 칸
		AddBinding(B, TAG_Stat_MaxStamina, TEXT("최대 스태미나"),
			Attr::GetPlayer_MaxManaAttribute(), TAG_Data_Equipment_MaxStamina_Flat,
			Attr::GetPlayer_MaxManaAttribute(), TAG_Data_Equipment_MaxStamina_Percent, Multiply);

		// ===== 칸에 배율 — % 는 배율에 더한다 =====
		// + 를 배율 칸에 넣으면 1.0 + 10 = 11배라 따로 둔 고정치 칸으로 보낸다. 없으면 NoAttribute.

		// 공격속도 = 직업 배율(전사 1.0, 궁수 1.2) + 옵션%
		AddBinding(B, TAG_Stat_AttackSpeed, TEXT("공격 속도"),
			NoAttribute, NoTag,
			Attr::GetPlayer_AttackSpeedAttribute(), TAG_Data_Equipment_AttackSpeed_Percent, Add);

		// 이동속도 = (걷기/달리기 + 옵션+) × (1.0 + 옵션%). +10 은 진짜 속도 10
		AddBinding(B, TAG_Stat_MoveSpeed, TEXT("이동 속도"),
			Attr::GetPlayer_MoveSpeedFlatAttribute(), TAG_Data_Equipment_MoveSpeed_Flat,
			Attr::GetPlayer_MoveSpeedMultiplierAttribute(), TAG_Data_Equipment_MoveSpeed_Percent, Add);

		// 스태미나 회복 = (초당 회복 + 옵션+) × (1.0 + 옵션%)
		AddBinding(B, TAG_Stat_StaminaRegen, TEXT("스태미나 회복"),
			Attr::GetPlayer_ManaRegenFlatAttribute(), TAG_Data_Equipment_StaminaRegen_Flat,
			Attr::GetPlayer_ManaRegenMultiplierAttribute(), TAG_Data_Equipment_StaminaRegen_Percent, Add);

		// 쿨다운 = (스킬 쿨 + 옵션+) × (1.0 + 옵션%). 줄이려면 - 를 적는다
		AddBinding(B, TAG_Stat_Cooldown, TEXT("쿨다운"),
			Attr::GetPlayer_CooldownFlatAttribute(), TAG_Data_Equipment_Cooldown_Flat,
			Attr::GetPlayer_CooldownMultiplierAttribute(), TAG_Data_Equipment_Cooldown_Percent, Add);

		// 스킬 피해 = 스킬 피해 × (1.0 + 옵션%). 기본 공격엔 안 걸린다
		AddBinding(B, TAG_Stat_SkillDamage, TEXT("스킬 피해"),
			NoAttribute, NoTag,
			Attr::GetPlayer_SkillDamageMultiplierAttribute(), TAG_Data_Equipment_SkillDamage_Percent, Add);

		// 헤드샷 피해 = 머리 2배 × (1 + 옵션%). 칸은 0 에서 시작
		AddBinding(B, TAG_Stat_HeadshotDamage, TEXT("헤드샷 피해"),
			NoAttribute, NoTag,
			Attr::GetPlayer_HeadshotDamageBonusAttribute(), TAG_Data_Equipment_HeadshotDamage_Percent, Add);

		// 받는 피해 = 들어온 피해 × (1.0 + 옵션%). 덜 맞으려면 -. 최대 50% 감소
		AddBinding(B, TAG_Stat_DamageTaken, TEXT("받는 피해"),
			NoAttribute, NoTag,
			Attr::GetPlayer_DamageTakenMultiplierAttribute(), TAG_Data_Equipment_DamageTaken_Percent, Add);

		// 근접 받는 피해 = 근접으로 맞은 피해 × (1.0 + 옵션%). 위 받는 피해와 곱으로 겹친다
		AddBinding(B, TAG_Stat_MeleeDamageTaken, TEXT("근접 받는 피해"),
			NoAttribute, NoTag,
			Attr::GetPlayer_MeleeDamageTakenMultiplierAttribute(), TAG_Data_Equipment_MeleeDamageTaken_Percent, Add);

		// 회복 효과 = 모든 체력 회복량 × (1.0 + 옵션%). 포션·흡혈·전투 회복 전부
		AddBinding(B, TAG_Stat_HealingReceived, TEXT("회복 효과"),
			NoAttribute, NoTag,
			Attr::GetPlayer_HealingReceivedMultiplierAttribute(), TAG_Data_Equipment_HealingReceived_Percent, Add);

		// 초당 체력 회복 = (기본 재생 + 옵션+) × (1.0 + 옵션%). 스태미나 회복과 같은 모양
		// 기본 재생이 0 이라, % 만 있고 + 가 없으면 차지 않는다.
		AddBinding(B, TAG_Stat_HealthRegen, TEXT("초당 체력 회복"),
			Attr::GetPlayer_HealthRegenFlatAttribute(), TAG_Data_Equipment_HealthRegen_Flat,
			Attr::GetPlayer_HealthRegenMultiplierAttribute(), TAG_Data_Equipment_HealthRegen_Percent, Add);

		return B;
	}
}

const TArray<FItemStatBinding>& ItemStats::GetBindings()
{
	// 처음 부를 때 한 번 만든다. 표는 실행 중에 안 바뀐다.
	static const TArray<FItemStatBinding> Bindings = BuildBindings();
	return Bindings;
}

const FItemStatBinding* ItemStats::FindBinding(const FGameplayTag& Stat)
{
	for (const FItemStatBinding& Binding : GetBindings())
	{
		if (Binding.Stat == Stat)
		{
			return &Binding;
		}
	}
	return nullptr;
}

FItemStatTotal ItemStats::GetTotal(const FItemStatTotals& Totals, const FGameplayTag& Stat)
{
	if (const FItemStatTotal* Found = Totals.Find(Stat))
	{
		return *Found;
	}
	return FItemStatTotal();
}

float ItemStats::MakePercentMagnitude(const FItemStatBinding& Binding, float PercentSum)
{
	// 배율 칸에 더할 수. +10% → 0.1
	if (Binding.PercentMode == EItemPercentMode::AddToRatio)
	{
		return PercentSum;
	}

	// 칸 값에 곱할 수. +10% → 1.1,  -10% → 0.9
	// -100% 아래로 가면 스탯이 음수가 되므로 0 에서 자른다.
	return FMath::Max(0.f, 1.f + PercentSum);
}

bool ItemStats::AddOptionToTotals(const FItemOption& Option, FItemStatTotals& Totals)
{
	const FItemStatBinding* Binding = FindBinding(Option.Stat);
	if (!Binding)
	{
		return false;   // 표에 없는 스탯
	}

	if (Option.ValueType == EItemOptionValueType::Flat)
	{
		// "공격속도 +10" 처럼 받을 칸이 없는 조합
		if (!Binding->FlatAttribute.IsValid())
		{
			return false;
		}
		Totals.FindOrAdd(Option.Stat).Flat += Option.Value;
		return true;
	}

	if (!Binding->PercentAttribute.IsValid())
	{
		return false;
	}

	// DT 에는 10 = 10% 로 적는다
	Totals.FindOrAdd(Option.Stat).Percent += Option.Value / 100.f;
	return true;
}

void ItemStats::FillSpec(FGameplayEffectSpec& Spec, const FItemStatTotals& Totals)
{
	// GE 생성자가 같은 표를 돌며 칸을 만들었으므로 여기서 빠지는 칸이 없다.
	for (const FItemStatBinding& Binding : GetBindings())
	{
		const FItemStatTotal Total = GetTotal(Totals, Binding.Stat);

		if (Binding.FlatAttribute.IsValid())
		{
			Spec.SetSetByCallerMagnitude(Binding.FlatDataTag, Total.Flat);
		}

		if (Binding.PercentAttribute.IsValid())
		{
			Spec.SetSetByCallerMagnitude(Binding.PercentDataTag, MakePercentMagnitude(Binding, Total.Percent));
		}
	}
}

FString ItemStats::FormatOption(const FItemOption& Option)
{
	// 표에 없는 스탯이면 태그 이름을 그대로 찍는다. 설정 실수가 툴팁에서 보이게.
	FString Name = Option.Stat.ToString();
	if (const FItemStatBinding* Binding = FindBinding(Option.Stat))
	{
		Name = Binding->DisplayName;
	}

	FString Sign = TEXT("+");
	if (Option.Value < 0.f)
	{
		Sign = TEXT("-");
	}

	FString Suffix;
	if (Option.ValueType == EItemOptionValueType::Percent)
	{
		Suffix = TEXT("%");
	}

	const FString Number = FText::AsNumber(FMath::Abs(Option.Value)).ToString();
	return FString::Printf(TEXT("%s %s%s%s"), *Name, *Sign, *Number, *Suffix);
}

FString ItemStats::FormatLines(const TArray<FItemOption>& Options, const TArray<FItemEffect>& Effects)
{
	FString Text;

	for (const FItemOption& Option : Options)
	{
		AppendLine(Text, FormatOption(Option));
	}

	for (const FItemEffect& Effect : Effects)
	{
		FResolvedItemEffect Resolved;
		if (!ItemEffectRules::Resolve(Effect, Resolved))
		{
			// 정의를 안 골랐거나 다른 표를 고른 경우. 설정 실수가 툴팁에서 보이게 한다.
			AppendLine(Text, TEXT("(효과 정의 없음)"));
			continue;
		}
		if (Resolved.Def.Description.IsEmpty())
		{
			continue;
		}

		// 숫자를 DT 에서 고쳐도 문구가 따라오게 한다
		FString Line = Resolved.Def.Description;
		Line.ReplaceInline(TEXT("{Amount}"), *FText::AsNumber(Resolved.Amount).ToString());
		Line.ReplaceInline(TEXT("{Condition}"), *FText::AsNumber(Resolved.Condition).ToString());
		AppendLine(Text, Line);
	}

	return Text;
}
