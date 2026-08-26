#include "Player/Job/HBJobData.h"

#include "Abilities/GameplayAbility.h"

namespace
{
	TArray<FHBJobSkillData> BuildLegacyCatalog(
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses)
	{
		TArray<FHBJobSkillData> Result;
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
		{
			if (!AbilityClass) continue;
			FHBJobSkillData Skill;
			Skill.SkillId = AbilityClass->GetFName();
			Skill.DisplayName = FText::FromString(AbilityClass->GetName());
			Skill.AbilityClass = AbilityClass;
			Result.Add(MoveTemp(Skill));
		}
		return Result;
	}

	const FHBJobSkillData* FindSkill(const TArray<FHBJobSkillData>& Skills, const FName SkillId)
	{
		return Skills.FindByPredicate([SkillId](const FHBJobSkillData& Skill)
		{
			return Skill.SkillId == SkillId;
		});
	}
}

TArray<FHBJobSkillData> UHBJobData::GetBasicSkillCatalog() const
{
	return BasicSkills.IsEmpty() ? BuildLegacyCatalog(BasicAbilities) : BasicSkills;
}

TArray<FHBJobSkillData> UHBJobData::GetActiveSkillCatalog() const
{
	return ActiveSkills.IsEmpty() ? BuildLegacyCatalog(ActiveAbilities) : ActiveSkills;
}

TArray<FHBJobSkillData> UHBJobData::GetPassiveSkillCatalog() const
{
	return PassiveSkills.IsEmpty() ? BuildLegacyCatalog(PassiveAbilities) : PassiveSkills;
}

const FHBJobSkillData* UHBJobData::FindActiveSkill(const FName SkillId) const
{
	if (!ActiveSkills.IsEmpty()) return FindSkill(ActiveSkills, SkillId);
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : ActiveAbilities)
	{
		if (AbilityClass && AbilityClass->GetFName() == SkillId)
		{
			static thread_local FHBJobSkillData LegacySkill;
			LegacySkill.SkillId = SkillId;
			LegacySkill.AbilityClass = AbilityClass;
			return &LegacySkill;
		}
	}
	return nullptr;
}

const FHBJobSkillData* UHBJobData::FindPassiveSkill(const FName SkillId) const
{
	if (!PassiveSkills.IsEmpty()) return FindSkill(PassiveSkills, SkillId);
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : PassiveAbilities)
	{
		if (AbilityClass && AbilityClass->GetFName() == SkillId)
		{
			static thread_local FHBJobSkillData LegacySkill;
			LegacySkill.SkillId = SkillId;
			LegacySkill.AbilityClass = AbilityClass;
			return &LegacySkill;
		}
	}
	return nullptr;
}
