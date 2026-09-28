#include "Combat/Ability/HBCommonAugmentSet.h"

const FHBSkillAugment* UHBCommonAugmentSet::FindAugment(const FGameplayTag AugmentTag) const
{
	if (!AugmentTag.IsValid())
	{
		return nullptr;
	}
	for (const FHBSkillAugment& Augment : Augments)
	{
		if (Augment.AugmentTag == AugmentTag)
		{
			return &Augment;
		}
	}
	return nullptr;
}
