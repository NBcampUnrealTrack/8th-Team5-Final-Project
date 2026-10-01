#include "Player/Job/HBSkillDescription.h"

#include "GameplayEffect.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"

namespace
{
	// ================ 목록의 어휘 ================

	enum class EValueSource : uint8
	{
		AbilityField,          // GA 의 Cooldown / ManaCost / BuffDuration
		SkillOwn,              // GA 가 직접 답한다. 패링처럼 어트리뷰트도 GE 도 아닌 값
		SkillEffectModifier,   // 액티브 SelfBuffEffect / 패시브 SelfPassiveEffect 의 모디파이어

		// 아래 셋은 선언만 해둔다. 이번에는 목록에 쓰는 항목이 없다.
		AugmentParam,
		AugmentGrantModifier,
		AugmentBuffModifier,
	};

	enum class EAbilityField : uint8
	{
		None,
		Cooldown,
		ManaCost,
		BuffDuration,
	};

	/**
	 * 이 수치를 어떻게 읽을 것인가.
	 *
	 * Mod* 둘은 모디파이어 출처 전용이다. 연산자가 의미를 마저 정하므로
	 * 여기서는 "이 어트리뷰트가 담는 단위" 만 말한다.
	 * 나머지는 연산자가 없는 출처라 이것이 곧 의미다.
	 */
	enum class EValueKind : uint8
	{
		ModAbsolute,   // 체력, 공격력처럼 값이 곧 수치
		ModRatio,      // 이동속도배율, 흡혈률처럼 1.0 이나 0 이 기준인 비율

		Flat,          // 20   -> 20
		Seconds,       // 0.3  -> 0.3
		Ratio,         // 0.9  -> 90%
		ScaleDown,     // 0.7  -> 30%   증강 전용. 이번 미사용
	};

	struct FValueDef
	{
		EValueSource Source = EValueSource::SkillEffectModifier;
		EValueKind   Kind = EValueKind::ModAbsolute;

		// Source 가 모디파이어 계열일 때만 본다.
		FGameplayAttribute Attribute;

		// Source 가 AbilityField 일 때만 본다.
		EAbilityField Field = EAbilityField::None;
	};

	// ================ 목록 ================

	void AddAttribute(TMap<FName, FValueDef>& InOutTable, const TCHAR* InName,
		const FGameplayAttribute& InAttribute, const EValueKind InKind)
	{
		FValueDef Def;
		Def.Source = EValueSource::SkillEffectModifier;
		Def.Kind = InKind;
		Def.Attribute = InAttribute;
		InOutTable.Add(InName, Def);
	}

	void AddField(TMap<FName, FValueDef>& InOutTable, const TCHAR* InName,
		const EAbilityField InField, const EValueKind InKind)
	{
		FValueDef Def;
		Def.Source = EValueSource::AbilityField;
		Def.Kind = InKind;
		Def.Field = InField;
		InOutTable.Add(InName, Def);
	}

	void AddOwn(TMap<FName, FValueDef>& InOutTable, const TCHAR* InName, const EValueKind InKind)
	{
		FValueDef Def;
		Def.Source = EValueSource::SkillOwn;
		Def.Kind = InKind;
		InOutTable.Add(InName, Def);
	}

	/**
	 * 쓸 수 있는 치환자 이름 전부. 이름을 늘리려면 여기 한 줄만 추가한다.
	 *
	 * 대소문자는 FName 이 알아서 무시하므로 health 와 Health 가 같다.
	 * 어트리뷰트 조회는 리플렉션이 올라온 뒤여야 하므로 정적 초기화가 아니라
	 * 첫 호출 때 한 번 만든다.
	 */
	TMap<FName, FValueDef> BuildRegistry()
	{
		TMap<FName, FValueDef> Table;

		// ---- 스킬의 버프 GE 모디파이어에서 읽는 것 ----
		AddAttribute(Table, TEXT("Health"),
			UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute(), EValueKind::ModAbsolute);
		AddAttribute(Table, TEXT("Attack"),
			UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute(), EValueKind::ModAbsolute);
		AddAttribute(Table, TEXT("Defense"),
			UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute(), EValueKind::ModAbsolute);

		AddAttribute(Table, TEXT("MoveSpeed"),
			UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(), EValueKind::ModRatio);
		AddAttribute(Table, TEXT("AttackSpeed"),
			UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute(), EValueKind::ModRatio);
		AddAttribute(Table, TEXT("LifeSteal"),
			UHB_PlayerAttributeSet::GetPlayer_LifeStealRatioAttribute(), EValueKind::ModRatio);
		AddAttribute(Table, TEXT("LowHealthBonus"),
			UHB_PlayerAttributeSet::GetPlayer_LowHealthAttackBonusAttribute(), EValueKind::ModRatio);
		AddAttribute(Table, TEXT("LowHealthThreshold"),
			UHB_PlayerAttributeSet::GetPlayer_LowHealthAttackThresholdAttribute(), EValueKind::ModRatio);
		AddAttribute(Table, TEXT("SprintMana"),
			UHB_PlayerAttributeSet::GetPlayer_SprintManaCostMultiplierAttribute(), EValueKind::ModRatio);

		// ---- GA 애셋의 칸에서 읽는 것 ----
		AddField(Table, TEXT("Duration"), EAbilityField::BuffDuration, EValueKind::Seconds);
		AddField(Table, TEXT("Cooldown"), EAbilityField::Cooldown, EValueKind::Seconds);
		AddField(Table, TEXT("Mana"), EAbilityField::ManaCost, EValueKind::Flat);

		// ---- 스킬이 직접 답하는 것 ----
		AddOwn(Table, TEXT("ParryWindow"), EValueKind::Seconds);
		AddOwn(Table, TEXT("ParryReduction"), EValueKind::Ratio);

		return Table;
	}

	const TMap<FName, FValueDef>& GetRegistry()
	{
		static const TMap<FName, FValueDef> Registry = BuildRegistry();
		return Registry;
	}

	// ================ 값 읽기 ================

	const UHBPlayerSkillGABase* GetSkillCDO(const FHBSkillValueContext& InContext)
	{
		if (!InContext.AbilityClass)
		{
			return nullptr;
		}
		return Cast<UHBPlayerSkillGABase>(InContext.AbilityClass->GetDefaultObject());
	}

	// 이 스킬이 거는 GE. 액티브와 패시브가 서로 다른 칸에 들고 있다.
	// 한쪽만 보면 최대 체력이나 흡혈 설명이 통째로 안 나온다.
	TSubclassOf<UGameplayEffect> GetSkillEffect(const UHBPlayerSkillGABase& InSkillCDO)
	{
		const UHBPlayerActiveGABase* ActiveCDO = Cast<UHBPlayerActiveGABase>(&InSkillCDO);
		if (ActiveCDO)
		{
			return ActiveCDO->GetSelfBuffEffect();
		}

		const UHBPlayerPassiveGABase* PassiveCDO = Cast<UHBPlayerPassiveGABase>(&InSkillCDO);
		if (PassiveCDO)
		{
			return PassiveCDO->GetSelfPassiveEffect();
		}

		return nullptr;
	}

	bool ReadAbilityField(const UHBPlayerActiveGABase& InActiveCDO, const EAbilityField InField,
		float& OutValue, FString& OutReason)
	{
		if (InField == EAbilityField::Cooldown)
		{
			OutValue = InActiveCDO.GetBaseCooldown();
			return true;
		}
		if (InField == EAbilityField::ManaCost)
		{
			OutValue = InActiveCDO.GetBaseManaCost();
			return true;
		}
		if (InField == EAbilityField::BuffDuration)
		{
			OutValue = InActiveCDO.GetBaseBuffDuration();
			return true;
		}

		OutReason = TEXT("목록의 칸 지정이 비어 있다");
		return false;
	}

	/**
	 * GE 에서 이 어트리뷰트를 건드리는 모디파이어 하나를 찾는다.
	 *
	 * 첫 번째를 집지 않고 전부 센다. 둘 이상이면 어느 것이 설명의 주인공인지
	 * 코드가 알 수 없고, 하나를 골라 보여주면 그럴듯한 오답이 된다.
	 * 읽을 수 없는 모디파이어를 건너뛰고 다른 것을 찾지도 않는다. 같은 이유다.
	 */
	bool FindModifier(const TSubclassOf<UGameplayEffect> InEffectClass, const FGameplayAttribute& InAttribute,
		float& OutValue, EGameplayModOp::Type& OutOp, FString& OutReason)
	{
		if (!InEffectClass)
		{
			OutReason = TEXT("이 스킬에 GE 가 지정되어 있지 않다");
			return false;
		}

		const UGameplayEffect* EffectCDO = InEffectClass->GetDefaultObject<UGameplayEffect>();
		if (!EffectCDO)
		{
			OutReason = TEXT("GE 의 CDO 를 얻지 못했다");
			return false;
		}

		int32 MatchCount = 0;
		const FGameplayModifierInfo* Found = nullptr;

		for (const FGameplayModifierInfo& Modifier : EffectCDO->Modifiers)
		{
			if (Modifier.Attribute != InAttribute)
			{
				continue;
			}
			MatchCount++;
			Found = &Modifier;
		}

		if (MatchCount == 0)
		{
			OutReason = TEXT("GE 에 그 어트리뷰트를 건드리는 모디파이어가 없다");
			return false;
		}
		if (MatchCount > 1)
		{
			OutReason = TEXT("같은 어트리뷰트를 건드리는 모디파이어가 둘 이상이다");
			return false;
		}

		float Magnitude = 0.f;
		// SetByCaller, AttributeBased, 커스텀 계산은 애셋에 숫자가 없다.
		if (!Found->ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Magnitude))
		{
			OutReason = TEXT("모디파이어 크기를 애셋에서 읽을 수 없다 (SetByCaller 등)");
			return false;
		}

		OutValue = Magnitude;
		OutOp = Found->ModifierOp;
		return true;
	}

	bool ReadRawValue(const FName InName, const FValueDef& InDef, const FHBSkillValueContext& InContext,
		float& OutValue, EGameplayModOp::Type& OutOp, FString& OutReason)
	{
		// 모디파이어 출처가 아니면 연산자가 없다. 덧셈으로 둬야 아래 변환이 맞다.
		OutOp = EGameplayModOp::Additive;

		if (InDef.Source == EValueSource::AugmentParam
			|| InDef.Source == EValueSource::AugmentGrantModifier
			|| InDef.Source == EValueSource::AugmentBuffModifier)
		{
			OutReason = TEXT("증강 출처는 아직 지원하지 않는다");
			return false;
		}

		const UHBPlayerSkillGABase* SkillCDO = GetSkillCDO(InContext);
		if (!SkillCDO)
		{
			OutReason = TEXT("스킬 GA 를 얻지 못했다");
			return false;
		}

		if (InDef.Source == EValueSource::SkillOwn)
		{
			if (!SkillCDO->GetNamedSkillValue(InName, OutValue))
			{
				OutReason = TEXT("이 스킬이 그 이름의 수치를 답하지 않는다");
				return false;
			}
			return true;
		}

		if (InDef.Source == EValueSource::AbilityField)
		{
			const UHBPlayerActiveGABase* ActiveCDO = Cast<UHBPlayerActiveGABase>(SkillCDO);
			if (!ActiveCDO)
			{
				OutReason = TEXT("패시브에는 쿨타임, 마나, 지속시간 칸이 없다");
				return false;
			}
			return ReadAbilityField(*ActiveCDO, InDef.Field, OutValue, OutReason);
		}

		return FindModifier(GetSkillEffect(*SkillCDO), InDef.Attribute, OutValue, OutOp, OutReason);
	}

	// ================ 표시로 바꾸기 ================

	/** 단위와 연산자로 표시 값을 만든다. 백분율인지도 여기서 정해진다. */
	bool ApplyKind(const EValueKind InKind, const EGameplayModOp::Type InOp,
		float& InOutValue, bool& bOutPercent, FString& OutReason)
	{
		if (InKind == EValueKind::Flat || InKind == EValueKind::Seconds)
		{
			bOutPercent = false;
			return true;
		}
		if (InKind == EValueKind::Ratio)
		{
			InOutValue = InOutValue * 100.f;
			bOutPercent = true;
			return true;
		}
		if (InKind == EValueKind::ScaleDown)
		{
			InOutValue = (1.f - InOutValue) * 100.f;
			bOutPercent = true;
			return true;
		}

		// 여기부터는 모디파이어 출처다. 연산자가 의미를 마저 정한다.
		if (InOp == EGameplayModOp::Multiplicitive)
		{
			// 배율은 1.0 이 기준점이다. 기존 값 대비 상대 변화율을 준다.
			InOutValue = (InOutValue - 1.f) * 100.f;
			bOutPercent = true;
			return true;
		}

		// 덮어쓰기는 그 값 자체가 최종값이다. 변화량이 아니다.
		// "체력 30% 이하일 때" 처럼 얼마가 된다를 말하는 수치가 이 방식이다.
		// 저체력 임계값이 그 예다. HB_PlayerAttributeSet.cpp 의 중립값 주석 참조.
		if (InOp == EGameplayModOp::Override)
		{
			if (InKind == EValueKind::ModRatio)
			{
				InOutValue = InOutValue * 100.f;
				bOutPercent = true;
				return true;
			}

			bOutPercent = false;
			return true;
		}

		if (InOp != EGameplayModOp::Additive)
		{
			OutReason = TEXT("지원하지 않는 모디파이어 연산자다 (나눗셈 등)");
			return false;
		}

		// 덧셈이다. 비율 속성이면 백분율, 절대값 속성이면 값 그대로.
		if (InKind == EValueKind::ModRatio)
		{
			InOutValue = InOutValue * 100.f;
			bOutPercent = true;
			return true;
		}

		bOutPercent = false;
		return true;
	}

	FString FormatNumber(const float InValue, const bool bInSign, const bool bInPercent)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		// 0.3 초 같은 값을 살리면서 19.999998 을 20 으로 만든다.
		Options.MaximumFractionalDigits = 2;

		FString Number = FText::AsNumber(InValue, &Options).ToString();

		// 음수는 이미 부호를 달고 나온다. 양수에만 붙인다.
		if (bInSign && InValue > 0.f)
		{
			Number = TEXT("+") + Number;
		}
		if (bInPercent)
		{
			Number += TEXT("%");
		}
		return Number;
	}

	/**
	 * 리치 텍스트가 태그로 읽을 기호를 막는다.
	 *
	 * & 를 먼저 바꿔야 한다. 나중에 하면 &lt; 의 & 를 다시 바꿔 &amp;lt; 가 된다.
	 */
	FString EscapeRichText(const FString& InText)
	{
		FString Out = InText;
		Out.ReplaceInline(TEXT("&"), TEXT("&amp;"), ESearchCase::CaseSensitive);
		Out.ReplaceInline(TEXT("<"), TEXT("&lt;"), ESearchCase::CaseSensitive);
		Out.ReplaceInline(TEXT(">"), TEXT("&gt;"), ESearchCase::CaseSensitive);
		return Out;
	}

	void ReportFailure(const FHBSkillValueContext& InContext, const FString& InToken, const FString& InReason)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillDesc] %s | {%s} 를 읽지 못했다: %s"),
			*InContext.DebugSkillId.ToString(), *InToken, *InReason);
	}

	bool ResolveToken(const FString& InToken, const FHBSkillValueContext& InContext, FString& OutNumber)
	{
		FString Name = InToken;
		bool bWantSign = false;
		bool bWantDrop = false;
		bool bWantPercent = false;

		if (Name.StartsWith(TEXT("+")))
		{
			bWantSign = true;
			Name.RightChopInline(1);
		}
		else if (Name.StartsWith(TEXT("-")))
		{
			bWantDrop = true;
			Name.RightChopInline(1);
		}

		if (Name.EndsWith(TEXT("%")))
		{
			bWantPercent = true;
			Name.LeftChopInline(1);
		}

		Name.TrimStartAndEndInline();

		if (Name.IsEmpty())
		{
			ReportFailure(InContext, InToken, TEXT("이름이 비어 있다"));
			return false;
		}

		const FValueDef* Def = GetRegistry().Find(FName(*Name));
		if (!Def)
		{
			ReportFailure(InContext, InToken, TEXT("목록에 없는 이름이다"));
			return false;
		}

		float Value = 0.f;
		EGameplayModOp::Type Op = EGameplayModOp::Additive;
		FString Reason;

		if (!ReadRawValue(FName(*Name), *Def, InContext, Value, Op, Reason))
		{
			ReportFailure(InContext, InToken, Reason);
			return false;
		}

		bool bPercent = false;
		if (!ApplyKind(Def->Kind, Op, Value, bPercent, Reason))
		{
			ReportFailure(InContext, InToken, Reason);
			return false;
		}

		// 표기 검증. 목록이 사실이고 표기는 저자의 기대다.
		// 어긋나면 치환하지 않는다. Flat 에 % 를 붙여 5000% 가 나오는 사고를 막는다.
		if (bPercent && !bWantPercent)
		{
			ReportFailure(InContext, InToken, TEXT("백분율인데 % 표기가 없다"));
			return false;
		}
		if (!bPercent && bWantPercent)
		{
			ReportFailure(InContext, InToken, TEXT("백분율이 아닌데 % 표기를 썼다"));
			return false;
		}

		// 감소 여부 검사는 백분율 변환 뒤, 반올림 전에 한다.
		// 반올림 뒤에 하면 아주 작은 증가가 0 으로 뭉개져 감소로 통과한다.
		if (bWantDrop)
		{
			if (Value >= 0.f)
			{
				ReportFailure(InContext, InToken, TEXT("- 를 썼는데 값이 감소가 아니다"));
				return false;
			}
			Value = -Value;
		}

		OutNumber = FormatNumber(Value, bWantSign, bPercent);
		return true;
	}
}

FHBFormattedText FormatSkillText(const FText& InText, const FHBSkillValueContext& InContext)
{
	FHBFormattedText Result;

	const FString Raw = InText.ToString();

	// 치환자가 없어도 리치 쪽은 이스케이프를 거쳐야 한다.
	// "조건 A < B" 같은 문장이 그냥 나가면 < B 가 태그로 먹힌다.
	if (!Raw.Contains(TEXT("{")))
	{
		Result.Plain = InText;
		Result.Rich = FText::FromString(EscapeRichText(Raw));
		return Result;
	}

	FString Plain;
	FString Rich;
	Plain.Reserve(Raw.Len() + 32);
	Rich.Reserve(Raw.Len() + 96);

	int32 Cursor = 0;
	while (true)
	{
		const int32 Open = Raw.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (Open == INDEX_NONE)
		{
			const FString Tail = Raw.Mid(Cursor);
			Plain += Tail;
			Rich += EscapeRichText(Tail);
			break;
		}

		const int32 Close = Raw.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
		if (Close == INDEX_NONE)
		{
			// 닫는 괄호가 없다. 남은 것은 전부 그냥 글자다.
			const FString Tail = Raw.Mid(Cursor);
			Plain += Tail;
			Rich += EscapeRichText(Tail);
			break;
		}

		const FString Before = Raw.Mid(Cursor, Open - Cursor);
		Plain += Before;
		Rich += EscapeRichText(Before);

		FString Number;
		if (ResolveToken(Raw.Mid(Open + 1, Close - Open - 1), InContext, Number))
		{
			Plain += Number;
			// 값에만 색을 준다. 손으로 적은 숫자와 눈으로 구분된다.
			Rich += TEXT("<Value>");
			Rich += Number;
			Rich += TEXT("</>");
		}
		else
		{
			// 못 읽은 치환자는 중괄호째 남긴다.
			// 임의의 숫자로 대체하면 조용히 틀린 설명이 된다.
			const FString Kept = Raw.Mid(Open, Close - Open + 1);
			Plain += Kept;
			Rich += EscapeRichText(Kept);
		}

		Cursor = Close + 1;
	}

	Result.Plain = FText::FromString(Plain);
	Result.Rich = FText::FromString(Rich);
	return Result;
}
