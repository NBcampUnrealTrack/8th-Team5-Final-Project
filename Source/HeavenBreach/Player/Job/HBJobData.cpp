#include "Player/Job/HBJobData.h"

#include "Abilities/GameplayAbility.h"

namespace
{
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

	return FormatSkillText(Description, Context);
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
