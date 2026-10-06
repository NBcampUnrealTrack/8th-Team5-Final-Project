#include "Combat/Ability/Passive/GA_PassiveLightLanding.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBLightLandingAugmentTest, "HeavenBreach.FallDamage.LightLandingAugment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBLightLandingAugmentTest::RunTest(const FString&)
{
	const UGA_PassiveLightLanding* CDO = GetDefault<UGA_PassiveLightLanding>();
	TestEqual(TEXT("기본 효과는 가벼운 착지 GE"), CDO->GetSelfPassiveEffect().Get(), UGE_LightLandingPassive::StaticClass());

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("강화 증강 1개"), Augments.Num(), 1))
	{
		return false;
	}

	const FHBSkillAugment& Augment = Augments[0];
	TestEqual(TEXT("증강 태그"), Augment.AugmentTag, AbilityTag::TAG_Augment_Passive_LightLanding_Reduction.GetTag());
	TestEqual(TEXT("3단계"), Augment.TierMagnitudes.Num(), 3);
	if (Augment.TierMagnitudes.Num() == 3)
	{
		TestEqual(TEXT("1단계 +0.15"), Augment.TierMagnitudes[0], 0.15f);
		TestEqual(TEXT("2단계 +0.25"), Augment.TierMagnitudes[1], 0.25f);
		TestEqual(TEXT("3단계 +0.35"), Augment.TierMagnitudes[2], 0.35f);
	}
	TestEqual(TEXT("증강 GE"), Augment.GrantEffect.Get(), UGE_LightLandingAugment::StaticClass());
	TestFalse(TEXT("이름 있음"), Augment.DisplayName.IsEmpty());

	return true;
}

#endif
