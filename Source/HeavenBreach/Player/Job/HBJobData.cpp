#include "Player/Job/HBJobData.h"

#include "Abilities/GameplayAbility.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Core/Tag/Item/HB_ItemTag.h"

namespace
{
	// 무기 태그 → 화면에 쓸 이름. 표에 없는 태그는 태그 이름을 그대로 쓴다 — 설정 실수가 화면에서 보이게.
	FString GetWeaponDisplayName(const FGameplayTag& WeaponTag)
	{
		if (WeaponTag == ItemTag::TAG_Weapon_Sword)
		{
			return TEXT("검");
		}
		if (WeaponTag == ItemTag::TAG_Weapon_Bow)
		{
			return TEXT("활");
		}
		if (WeaponTag == ItemTag::TAG_Weapon_Staff)
		{
			return TEXT("지팡이");
		}
		if (WeaponTag == ItemTag::TAG_Weapon_Dagger)
		{
			return TEXT("단검");
		}
		if (WeaponTag == ItemTag::TAG_Weapon_Unarmed)
		{
			return TEXT("맨손");
		}
		// 부모 태그(Weapon)만 넣으면 아무 무기나 된다 (RequiredWeaponTags 규칙).
		if (WeaponTag.GetTagName() == FName(TEXT("Weapon")))
		{
			return TEXT("아무 무기");
		}
		return WeaponTag.ToString();
	}

	// 액티브 스킬의 필요 무기를 "필요 무기: 검" 한 줄로 만든다.
	// 무기 제약이 없거나 패시브(액티브 베이스가 아님)면 빈 문자열이다.
	//
	// 왜 설명에 손으로 안 적나: GA 의 RequiredWeaponTags 를 고치면 설명을 빠뜨려 UI 가 거짓말을 한다.
	// 쳐낼 조건: UI 에서 무기 조건을 따로 꾸미게 되면(색 · 아이콘 · 별도 줄) 카드 데이터 칸으로 옮긴다.
	FString MakeRequiredWeaponLine(const TSubclassOf<UGameplayAbility>& AbilityClass)
	{
		if (!AbilityClass)
		{
			return FString();
		}

		const UHBPlayerActiveGABase* ActiveCDO = Cast<UHBPlayerActiveGABase>(AbilityClass->GetDefaultObject());
		if (!ActiveCDO)
		{
			return FString();
		}

		TArray<FString> WeaponNames;
		for (const FGameplayTag& WeaponTag : ActiveCDO->GetRequiredWeaponTags())
		{
			WeaponNames.Add(GetWeaponDisplayName(WeaponTag));
		}

		if (WeaponNames.Num() == 0)
		{
			return FString();
		}

		return FString::Printf(TEXT("필요 무기: %s"), *FString::Join(WeaponNames, TEXT(" · ")));
	}

	// 설명 다음 줄에 덧붙인다. 설명이 비어 있으면 빈 줄 없이 그 줄만 남는다.
	FText AppendWeaponLine(const FText& Text, const FString& Line)
	{
		FString Result = Text.ToString();
		if (!Result.IsEmpty())
		{
			Result += TEXT("\n");
		}
		Result += Line;
		return FText::FromString(Result);
	}
		// 직업의 스킬데이터 안에서 스킬의 id 찾기
	const FHBJobSkillData* FindSkillById(const TArray<FHBJobSkillData>& Skills, const FName SkillId)
	{
		// 빈 슬롯(E 미지정등) 조회를 막는다.
		if (SkillId.IsNone())
		{
			return nullptr;
		}

		for (const FHBJobSkillData& Skill : Skills)
		{
			// 동일한 스킬 id발견시 반환
			if (Skill.SkillId == SkillId)
			{
				return &Skill;
			}
		}
		return nullptr;
	}
}

FHBFormattedText FHBJobSkillData::FormatDescription() const
{
	FHBSkillValueContext Context;
	Context.AbilityClass = AbilityClass;
	// 못 읽은 치환자가 어느 스킬 것인지 로그로 알아야 한다.
	Context.DebugSkillId = SkillId;

	FHBFormattedText Formatted = FormatSkillText(Description, Context);

	const FString WeaponLine = MakeRequiredWeaponLine(AbilityClass);
	if (WeaponLine.IsEmpty())
	{
		return Formatted;
	}

	// 평문 · 리치 둘 다 붙인다. 줄에 < > & 가 없어 리치 쪽 이스케이프는 필요 없다.
	Formatted.Plain = AppendWeaponLine(Formatted.Plain, WeaponLine);
	Formatted.Rich = AppendWeaponLine(Formatted.Rich, WeaponLine);
	return Formatted;
}

FText FHBJobSkillData::GetFormattedDescription() const
{
	return FormatDescription().Plain;
}

const FHBJobSkillData* UHBJobData::FindActiveSkill(const FName SkillId) const
{
	return FindSkillById(ActiveSkills, SkillId);
}

const FHBJobSkillData* UHBJobData::FindPassiveSkill(const FName SkillId) const
{
	return FindSkillById(PassiveSkills, SkillId);
}

const FHBJobSkillData* UHBJobData::FindSkillAnywhere(FName SkillId) const
{
	if (const FHBJobSkillData* Skill = FindSkillById(ActiveSkills, SkillId))
	{
		return Skill;
	}
	if (const FHBJobSkillData* Skill = FindSkillById(PassiveSkills, SkillId))
	{
		return Skill;
	}
	if (UltimateSkill.SkillId == SkillId)
	{
		return &UltimateSkill;
	}
	return nullptr;
}
