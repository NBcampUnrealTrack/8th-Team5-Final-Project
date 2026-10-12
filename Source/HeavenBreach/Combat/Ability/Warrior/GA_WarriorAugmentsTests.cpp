// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Melee/GA_MeleeBattleCry.h"
#include "Combat/Ability/Melee/GA_MeleeBloodFrenzy.h"
#include "Combat/Ability/Passive/GA_PassiveCombatInstinct.h"
#include "Combat/Ability/Passive/GA_PassiveIronHeart.h"
#include "Combat/Ability/Warrior/GA_PlayerLeapAttack.h"
#include "Combat/Ability/Warrior/GA_WarriorSmash.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Misc/AutomationTest.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 증강 하나의 공통 항목: 태그, 이름·설명, 3단계 값.
	void CheckAugment(FAutomationTestBase& Test, const FHBSkillAugment& Augment, const FGameplayTag Tag, const TArray<float>& Tiers)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s 태그"), *Tag.ToString()), Augment.AugmentTag, Tag);
		Test.TestFalse(TEXT("이름 있음"), Augment.DisplayName.IsEmpty());
		Test.TestFalse(TEXT("설명 있음"), Augment.Description.IsEmpty());
		Test.TestEqual(*FString::Printf(TEXT("%s 단계"), *Tag.ToString()), Augment.TierMagnitudes, Tiers);
	}

	// 효율 증강: 쿨다운·마나에 같은 배율이 붙는다.
	void CheckEfficiency(FAutomationTestBase& Test, const FHBSkillAugment& Augment, const FGameplayTag Tag)
	{
		CheckAugment(Test, Augment, Tag, { 0.85f, 0.75f, 0.65f });
		float Value = 0.f;
		Test.TestTrue(TEXT("효율 쿨다운 항목"), Augment.FindParam(SkillParamTag::TAG_SkillParam_Cooldown, Value));
		Test.TestTrue(TEXT("효율 마나 항목"), Augment.FindParam(SkillParamTag::TAG_SkillParam_ManaCost, Value));
	}

	// GE 의 모디파이어가 하나이고, 그 어트리뷰트·연산·SetByCaller 태그가 맞는가.
	void CheckSetByCallerGE(FAutomationTestBase& Test, const UGameplayEffect* Effect, const FGameplayAttribute& Attribute,
		const EGameplayModOp::Type Op, const FGameplayTag DataTag, const EGameplayEffectDurationType Duration)
	{
		const FString Name = GetNameSafe(Effect ? Effect->GetClass() : nullptr);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s 모디파이어 1개"), *Name), Effect->Modifiers.Num(), 1))
		{
			return;
		}
		const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
		Test.TestTrue(*FString::Printf(TEXT("%s 어트리뷰트"), *Name), Modifier.Attribute == Attribute);
		Test.TestEqual(*FString::Printf(TEXT("%s 연산"), *Name), static_cast<int32>(Modifier.ModifierOp), static_cast<int32>(Op));
		Test.TestEqual(*FString::Printf(TEXT("%s SetByCaller"), *Name), Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag, DataTag);
		Test.TestEqual(*FString::Printf(TEXT("%s 지속 정책"), *Name), static_cast<int32>(Effect->DurationPolicy), static_cast<int32>(Duration));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBWarriorActiveAugmentsTest, "HeavenBreach.Warrior.ActiveAugments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBWarriorActiveAugmentsTest::RunTest(const FString&)
{
	const TArray<FHBSkillAugment>& Cry = GetDefault<UGA_MeleeBattleCry>()->GetAugments();
	if (TestEqual(TEXT("전투의 함성 증강 3개"), Cry.Num(), 3))
	{
		CheckEfficiency(*this, Cry[0], WarriorTag::TAG_Augment_BattleCry_Efficiency);
		CheckAugment(*this, Cry[1], WarriorTag::TAG_Augment_BattleCry_Duration, { 1.25f, 1.5f, 1.75f });
		float Value = 0.f;
		TestTrue(TEXT("함성 지속 항목"), Cry[1].FindParam(SkillParamTag::TAG_SkillParam_Duration, Value));
		CheckAugment(*this, Cry[2], WarriorTag::TAG_Augment_BattleCry_Damage, { 1.10f, 1.20f, 1.30f });
		TestTrue(TEXT("함성 위력 버프 GE"), Cry[2].BuffEffect == UGE_WarriorBattleCryPowerAugment::StaticClass());
	}

	const TArray<FHBSkillAugment>& Frenzy = GetDefault<UGA_MeleeBloodFrenzy>()->GetAugments();
	if (TestEqual(TEXT("피의 광란 증강 3개"), Frenzy.Num(), 3))
	{
		CheckEfficiency(*this, Frenzy[0], WarriorTag::TAG_Augment_BloodFrenzy_Efficiency);
		CheckAugment(*this, Frenzy[1], WarriorTag::TAG_Augment_BloodFrenzy_Lifesteal, { 0.05f, 0.10f, 0.20f });
		TestTrue(TEXT("광란 흡혈 버프 GE"), Frenzy[1].BuffEffect == UGE_WarriorBloodFrenzyLifestealAugment::StaticClass());
		CheckAugment(*this, Frenzy[2], WarriorTag::TAG_Augment_BloodFrenzy_Duration, { 1.25f, 1.5f, 1.75f });
		float Value = 0.f;
		TestTrue(TEXT("광란 지속 항목"), Frenzy[2].FindParam(SkillParamTag::TAG_SkillParam_Duration, Value));
	}

	const TArray<FHBSkillAugment>& Leap = GetDefault<UGA_PlayerLeapAttack>()->GetAugments();
	if (TestEqual(TEXT("돌진 강타 증강 3개"), Leap.Num(), 3))
	{
		CheckEfficiency(*this, Leap[0], WarriorTag::TAG_Augment_ChargeAttack_Efficiency);
		CheckAugment(*this, Leap[1], WarriorTag::TAG_Augment_ChargeAttack_Reach, { 1.2f, 1.4f, 1.6f });
		float Value = 0.f;
		TestTrue(TEXT("돌진 거리 항목"), Leap[1].FindParam(SkillParamTag::TAG_SkillParam_Reach, Value));
		CheckAugment(*this, Leap[2], WarriorTag::TAG_Augment_ChargeAttack_Guard, { 0.80f, 0.65f, 0.50f });
		TestTrue(TEXT("돌진 가드 발동 중 GE"), Leap[2].DuringEffect == UGE_WarriorChargeGuardAugment::StaticClass());
	}

	const TArray<FHBSkillAugment>& Smash = GetDefault<UGA_WarriorSmash>()->GetAugments();
	if (TestEqual(TEXT("강타 증강 3개"), Smash.Num(), 3))
	{
		CheckEfficiency(*this, Smash[0], WarriorTag::TAG_Augment_Smash_Efficiency);
		CheckAugment(*this, Smash[1], WarriorTag::TAG_Augment_Smash_Damage, { 0.15f, 0.30f, 0.50f });
		TestTrue(TEXT("강타 위력 발동 중 GE"), Smash[1].DuringEffect == UGE_WarriorSmashPowerAugment::StaticClass());
		CheckAugment(*this, Smash[2], WarriorTag::TAG_Augment_Smash_Shield, { 0.5f, 0.75f, 1.0f });
		float Value = 0.f;
		TestTrue(TEXT("방패 강타 전환 비율 항목"), Smash[2].FindParam(SkillParamTag::TAG_SkillParam_ShieldRatio, Value));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBWarriorPassiveAugmentsTest, "HeavenBreach.Warrior.PassiveAugments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBWarriorPassiveAugmentsTest::RunTest(const FString&)
{
	const TArray<FHBSkillAugment>& Iron = GetDefault<UGA_PassiveIronHeart>()->GetAugments();
	if (TestEqual(TEXT("강철 심장 증강 1개"), Iron.Num(), 1))
	{
		CheckAugment(*this, Iron[0], MeleeTag::TAG_Augment_Passive_IronHeart_Boost, { 1.10f, 1.20f, 1.35f });
		TestTrue(TEXT("강철 심장 GrantEffect"), Iron[0].GrantEffect == UGE_IronHeartAugment::StaticClass());
	}

	const TArray<FHBSkillAugment>& Instinct = GetDefault<UGA_PassiveCombatInstinct>()->GetAugments();
	if (TestEqual(TEXT("전투 본능 증강 1개"), Instinct.Num(), 1))
	{
		CheckAugment(*this, Instinct[0], MeleeTag::TAG_Augment_Passive_CombatInstinct_Boost, { 0.10f, 0.20f, 0.30f });
		TestTrue(TEXT("전투 본능 GrantEffect"), Instinct[0].GrantEffect == UGE_CombatInstinctAugment::StaticClass());
	}

	// 부모가 상시 GE 를 정하지 않는다. BP 에 저장된 값을 덮지 않아야 한다.
	TestTrue(TEXT("강철 심장 상시 GE 는 BP 몫"), GetDefault<UGA_PassiveIronHeart>()->GetSelfPassiveEffect() == nullptr);
	TestTrue(TEXT("전투 본능 상시 GE 는 BP 몫"), GetDefault<UGA_PassiveCombatInstinct>()->GetSelfPassiveEffect() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBWarriorAugmentEffectsTest, "HeavenBreach.Warrior.AugmentEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBWarriorAugmentEffectsTest::RunTest(const FString&)
{
	using Attr = UHB_PlayerAttributeSet;
	const FGameplayTag Magnitude = AbilityTag::TAG_SetByCaller_AugmentMagnitude;

	CheckSetByCallerGE(*this, GetDefault<UGE_WarriorBattleCryPowerAugment>(), Attr::GetPlayer_AttackPowerAttribute(),
		EGameplayModOp::Multiplicitive, Magnitude, EGameplayEffectDurationType::HasDuration);
	CheckSetByCallerGE(*this, GetDefault<UGE_WarriorBloodFrenzyLifestealAugment>(), Attr::GetPlayer_LifeStealRatioAttribute(),
		EGameplayModOp::Additive, Magnitude, EGameplayEffectDurationType::HasDuration);
	CheckSetByCallerGE(*this, GetDefault<UGE_WarriorChargeGuardAugment>(), Attr::GetPlayer_DamageTakenMultiplierAttribute(),
		EGameplayModOp::Multiplicitive, Magnitude, EGameplayEffectDurationType::Infinite);
	CheckSetByCallerGE(*this, GetDefault<UGE_WarriorSmashPowerAugment>(), Attr::GetPlayer_SkillDamageMultiplierAttribute(),
		EGameplayModOp::Additive, Magnitude, EGameplayEffectDurationType::Infinite);
	CheckSetByCallerGE(*this, GetDefault<UGE_WarriorSmashShieldPower>(), Attr::GetPlayer_AttackPowerAttribute(),
		EGameplayModOp::Additive, AbilityTag::TAG_SetByCaller_Damage, EGameplayEffectDurationType::Infinite);
	CheckSetByCallerGE(*this, GetDefault<UGE_IronHeartAugment>(), Attr::GetPlayer_MaxHPAttribute(),
		EGameplayModOp::Multiplicitive, Magnitude, EGameplayEffectDurationType::Infinite);
	CheckSetByCallerGE(*this, GetDefault<UGE_CombatInstinctAugment>(), Attr::GetPlayer_LowHealthAttackBonusAttribute(),
		EGameplayModOp::Additive, Magnitude, EGameplayEffectDurationType::Infinite);

	// 버프 GE 는 자버프와 같은 SetByCaller.Duration 으로 지속시간을 받는다.
	const UGameplayEffect* Cry = GetDefault<UGE_WarriorBattleCryPowerAugment>();
	TestEqual(TEXT("함성 위력 지속 SetByCaller"), Cry->DurationMagnitude.GetSetByCallerFloat().DataTag, AbilityTag::TAG_SetByCaller_Duration.GetTag());
	return true;
}

#endif
