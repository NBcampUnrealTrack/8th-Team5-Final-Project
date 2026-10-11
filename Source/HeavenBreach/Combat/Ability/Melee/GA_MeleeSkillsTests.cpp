#include "Combat/Ability/Melee/GA_MeleeWhirlwind.h"
#include "Combat/Ability/Passive/GA_PassiveBloodthirst.h"
#include "Combat/Ability/Passive/GA_PassiveCounter.h"
#include "Combat/Ability/Passive/GA_PassiveSteadyStance.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 근접 액티브 스킬 증강 1개를 검사한다. 이후 근접 스킬 테스트가 같이 쓴다.
	[[maybe_unused]] void CheckMeleeActiveAugment(FAutomationTestBase& Test, const FHBSkillAugment& Augment, const FGameplayTag Tag, const FGameplayTag Param, const float ParamValue, const TArray<float>& Tiers)
	{
		Test.TestEqual(TEXT("증강 태그"), Augment.AugmentTag, Tag);
		Test.TestFalse(TEXT("이름 있음"), Augment.DisplayName.IsEmpty());
		Test.TestFalse(TEXT("설명 있음"), Augment.Description.IsEmpty());
		float Value = 0.f;
		Test.TestTrue(TEXT("Params 항목"), Augment.FindParam(Param, Value));
		Test.TestEqual(TEXT("Params 값"), Value, ParamValue);
		if (Test.TestEqual(TEXT("3단계"), Augment.TierMagnitudes.Num(), Tiers.Num()))
		{
			for (int32 i = 0; i < Tiers.Num(); ++i)
			{
				Test.TestEqual(*FString::Printf(TEXT("%d단계 값"), i + 1), Augment.TierMagnitudes[i], Tiers[i]);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMeleePassivesTest, "HeavenBreach.Melee.Passives", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMeleePassivesTest::RunTest(const FString&)
{
	TestEqual(TEXT("받아치기 GE"), GetDefault<UGA_PassiveCounter>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_CounterPassive::StaticClass()));
	TestEqual(TEXT("피의 갈증 GE"), GetDefault<UGA_PassiveBloodthirst>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_BloodthirstPassive::StaticClass()));
	TestEqual(TEXT("견고한 자세 GE"), GetDefault<UGA_PassiveSteadyStance>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_SteadyStancePassive::StaticClass()));

	TestTrue(TEXT("받아치기 태그"), GetDefault<UGE_CounterPassive>()->GetGrantedTags().HasTagExact(MeleeTag::TAG_State_Counter));
	TestTrue(TEXT("피의 갈증 태그"), GetDefault<UGE_BloodthirstPassive>()->GetGrantedTags().HasTagExact(MeleeTag::TAG_State_Bloodthirst));
	TestTrue(TEXT("견고한 자세 태그"), GetDefault<UGE_SteadyStancePassive>()->GetGrantedTags().HasTagExact(MeleeTag::TAG_State_SteadyStance));
	TestEqual(TEXT("받아치기 GE 모디파이어 없음"), GetDefault<UGE_CounterPassive>()->Modifiers.Num(), 0);
	TestEqual(TEXT("피의 갈증 GE 모디파이어 없음"), GetDefault<UGE_BloodthirstPassive>()->Modifiers.Num(), 0);
	TestEqual(TEXT("견고한 자세 GE 모디파이어 없음"), GetDefault<UGE_SteadyStancePassive>()->Modifiers.Num(), 0);

	TestEqual(TEXT("받아치기 증강 없음"), GetDefault<UGA_PassiveCounter>()->GetAugments().Num(), 0);

	const TArray<FHBSkillAugment>& Blood = GetDefault<UGA_PassiveBloodthirst>()->GetAugments();
	if (TestEqual(TEXT("피의 갈증 증강 1개"), Blood.Num(), 1))
	{
		TestEqual(TEXT("피의 갈증 증강 태그"), Blood[0].AugmentTag, MeleeTag::TAG_Augment_Passive_Bloodthirst_Boost.GetTag());
		TestEqual(TEXT("피의 갈증 단계(최종 비율)"), Blood[0].TierMagnitudes, TArray<float>({ 0.07f, 0.09f, 0.12f }));
		TestTrue(TEXT("피의 갈증 증강 GE 없음"), Blood[0].GrantEffect == nullptr);
		TestFalse(TEXT("이름 있음"), Blood[0].DisplayName.IsEmpty());
		TestFalse(TEXT("설명 있음"), Blood[0].Description.IsEmpty());
	}
	const TArray<FHBSkillAugment>& Steady = GetDefault<UGA_PassiveSteadyStance>()->GetAugments();
	if (TestEqual(TEXT("견고한 자세 증강 1개"), Steady.Num(), 1))
	{
		TestEqual(TEXT("견고한 자세 증강 태그"), Steady[0].AugmentTag, MeleeTag::TAG_Augment_Passive_SteadyStance_Boost.GetTag());
		TestEqual(TEXT("견고한 자세 단계(최종 배율)"), Steady[0].TierMagnitudes, TArray<float>({ 0.60f, 0.50f, 0.40f }));
		TestTrue(TEXT("견고한 자세 증강 GE 없음"), Steady[0].GrantEffect == nullptr);
		TestFalse(TEXT("이름 있음"), Steady[0].DisplayName.IsEmpty());
		TestFalse(TEXT("설명 있음"), Steady[0].Description.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMeleeCooldownEffectsTest, "HeavenBreach.Melee.CooldownEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMeleeCooldownEffectsTest::RunTest(const FString&)
{
	TestTrue(TEXT("회전 베기 쿨다운 태그"), GetDefault<UGE_MeleeWhirlwindCooldown>()->GetGrantedTags().HasTagExact(MeleeTag::TAG_Cooldown_Whirlwind));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMeleeWhirlwindDefaultsTest, "HeavenBreach.Melee.WhirlwindDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMeleeWhirlwindDefaultsTest::RunTest(const FString&)
{
	const UGA_MeleeWhirlwind* CDO = GetDefault<UGA_MeleeWhirlwind>();
	TestEqual(TEXT("스태미나 25"), CDO->GetBaseManaCost(), 25.f);
	TestEqual(TEXT("쿨다운 8초"), CDO->GetBaseCooldown(), 8.f);
	TestTrue(TEXT("검 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Sword));
	TestEqual(TEXT("판정 지연 0.4"), CDO->GetHitDelay(), 0.4f);
	TestEqual(TEXT("반경 250"), CDO->GetRadius(), 250.f);
	TestEqual(TEXT("피해 계수 1.8"), CDO->GetDamageCoef(), 1.8f);

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_MeleeWhirlwindCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(MeleeTag::TAG_Cooldown_Whirlwind));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckMeleeActiveAugment(*this, Augments[0], MeleeTag::TAG_Augment_Melee_Whirlwind_Power, SkillParamTag::TAG_SkillParam_Damage, 1.15f, { 1.15f, 1.30f, 1.50f });
	CheckMeleeActiveAugment(*this, Augments[1], MeleeTag::TAG_Augment_Melee_Whirlwind_Wide, SkillParamTag::TAG_SkillParam_Radius, 300.f, { 300.f, 350.f, 400.f });
	return true;
}

#endif
