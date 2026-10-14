#include "Combat/Ability/Bow/GA_BowEvasiveShot.h"
#include "Combat/Ability/Bow/GA_BowHunterMark.h"
#include "Combat/Ability/Bow/GA_BowPiercingShot.h"
#include "Combat/Ability/Bow/GA_BowTripleShot.h"
#include "Combat/Attack/HBBowShot.h"
#include "Combat/Ability/Passive/GA_PassiveSwiftStep.h"
#include "Combat/Ability/Passive/GA_PassiveWeakPoint.h"
#include "Combat/Ability/Passive/GA_PassiveHunterBreath.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 증강 1개짜리 패시브 CDO 를 검사한다. 세 패시브가 같은 형태라 한 곳에 모았다.
	bool CheckPassive(FAutomationTestBase& Test, const UHBPlayerPassiveGABase* CDO, const TSubclassOf<UGameplayEffect> ExpectedPassive,
		const FGameplayTag ExpectedTag, const TArray<float>& ExpectedTiers, const TSubclassOf<UGameplayEffect> ExpectedAugmentGE)
	{
		Test.TestEqual(TEXT("기본 효과"), CDO->GetSelfPassiveEffect().Get(), ExpectedPassive.Get());

		const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
		if (!Test.TestEqual(TEXT("증강 1개"), Augments.Num(), 1))
		{
			return false;
		}

		const FHBSkillAugment& Augment = Augments[0];
		Test.TestEqual(TEXT("증강 태그"), Augment.AugmentTag, ExpectedTag);
		Test.TestFalse(TEXT("이름 있음"), Augment.DisplayName.IsEmpty());
		Test.TestFalse(TEXT("설명 있음"), Augment.Description.IsEmpty());
		Test.TestEqual(TEXT("증강 GE"), Augment.GrantEffect.Get(), ExpectedAugmentGE.Get());
		if (Test.TestEqual(TEXT("3단계"), Augment.TierMagnitudes.Num(), ExpectedTiers.Num()))
		{
			for (int32 i = 0; i < ExpectedTiers.Num(); ++i)
			{
				Test.TestEqual(*FString::Printf(TEXT("%d단계 값"), i + 1), Augment.TierMagnitudes[i], ExpectedTiers[i]);
			}
		}
		return true;
	}

	// 액티브 스킬 증강 1개를 검사한다. 표식·회피 테스트가 같이 쓴다.
	void CheckActiveAugment(FAutomationTestBase& Test, const FHBSkillAugment& Augment, const FGameplayTag Tag, const FGameplayTag Param, const float ParamValue, const TArray<float>& Tiers)
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

	// 쿨다운·비용 GE 클래스와 쿨다운 태그. UI 쿨다운 조회와 CheckCooldown 이 이 태그를 본다.
	void CheckCostAndCooldown(FAutomationTestBase& Test, const UGameplayAbility* CDO, const UClass* ExpectedCooldownGE, const FGameplayTag ExpectedCooldownTag)
	{
		const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
		Test.TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == ExpectedCooldownGE);
		const UGameplayEffect* CostGE = CDO->GetCostGameplayEffect();
		Test.TestTrue(TEXT("비용 GE 클래스"), CostGE && CostGE->GetClass() == UGE_SkillManaCost::StaticClass());
		const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
		Test.TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(ExpectedCooldownTag));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowPassivesTest, "HeavenBreach.Bow.Passives", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowPassivesTest::RunTest(const FString&)
{
	CheckPassive(*this, GetDefault<UGA_PassiveSwiftStep>(), UGE_SwiftStepPassive::StaticClass(),
		ArcherTag::TAG_Augment_Passive_SwiftStep_Boost, { 0.05f, 0.10f, 0.15f }, UGE_SwiftStepAugment::StaticClass());
	CheckPassive(*this, GetDefault<UGA_PassiveWeakPoint>(), UGE_WeakPointPassive::StaticClass(),
		ArcherTag::TAG_Augment_Passive_WeakPoint_Boost, { 0.15f, 0.30f, 0.50f }, UGE_WeakPointAugment::StaticClass());
	CheckPassive(*this, GetDefault<UGA_PassiveHunterBreath>(), UGE_HunterBreathPassive::StaticClass(),
		ArcherTag::TAG_Augment_Passive_HunterBreath_Boost, { -0.10f, -0.20f, -0.30f }, UGE_HunterBreathAugment::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowCooldownEffectsTest, "HeavenBreach.Bow.CooldownEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowCooldownEffectsTest::RunTest(const FString&)
{
	// GAS 기본 GetCooldownTags 는 CDO 의 GetGrantedTags 를 읽는다. 여기서 태그가 안 보이면 쿨다운이 안 걸린다.
	const auto CheckGrant = [this](const TCHAR* Name, const UGameplayEffect* GE, const FGameplayTag Tag, const EGameplayEffectDurationType Duration)
	{
		TestEqual(*FString::Printf(TEXT("%s 지속 방식"), Name), GE->DurationPolicy, Duration);
		TestTrue(*FString::Printf(TEXT("%s 태그 부여"), Name), GE->GetGrantedTags().HasTagExact(Tag));
	};
	CheckGrant(TEXT("관통"), GetDefault<UGE_BowPiercingCooldown>(), ArcherTag::TAG_Cooldown_PiercingArrow, EGameplayEffectDurationType::HasDuration);
	CheckGrant(TEXT("표식"), GetDefault<UGE_BowHunterMarkCooldown>(), ArcherTag::TAG_Cooldown_TrackingMark, EGameplayEffectDurationType::HasDuration);
	CheckGrant(TEXT("회피"), GetDefault<UGE_BowEvasiveCooldown>(), ArcherTag::TAG_Cooldown_EvasiveShot, EGameplayEffectDurationType::HasDuration);
	CheckGrant(TEXT("사냥꾼의 표식"), GetDefault<UGE_HunterMark>(), ArcherTag::TAG_State_Tracked, EGameplayEffectDurationType::HasDuration);
	TestEqual(TEXT("마나 비용은 즉발"), GetDefault<UGE_SkillManaCost>()->DurationPolicy, EGameplayEffectDurationType::Instant);
	TestEqual(TEXT("마나 비용 모디파이어"), GetDefault<UGE_SkillManaCost>()->Modifiers.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowPiercingShotDefaultsTest, "HeavenBreach.Bow.PiercingShotDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowPiercingShotDefaultsTest::RunTest(const FString&)
{
	const UGA_BowPiercingShot* CDO = GetDefault<UGA_BowPiercingShot>();

	TestEqual(TEXT("쿨다운 8초"), CDO->GetBaseCooldown(), 8.f);
	TestEqual(TEXT("마나 25"), CDO->GetBaseManaCost(), 25.f);
	TestTrue(TEXT("좌클릭 입력을 받는다"), CDO->WantsAimInput());
	TestEqual(TEXT("관통 4명 + 박히는 5번째 = 최대 5명"), CDO->GetMaxPierceCount(), 4);
	TestTrue(TEXT("활 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Bow));
	TestTrue(TEXT("조준 중 태그"), CDO->GetActivationOwnedTags().HasTagExact(StateTag::TAG_State_Bow_SkillAiming));

	// 쿨다운 GE 와 그 GE 가 주는 태그. UI 쿨다운 조회와 CheckCooldown 이 이 태그를 본다.
	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_BowPiercingCooldown::StaticClass());
	const UGameplayEffect* CostGE = CDO->GetCostGameplayEffect();
	TestTrue(TEXT("비용 GE 클래스"), CostGE && CostGE->GetClass() == UGE_SkillManaCost::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(ArcherTag::TAG_Cooldown_PiercingArrow));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}

	const auto CheckAugment = [this](const FHBSkillAugment& Augment, const FGameplayTag Tag, const FGameplayTag Param, const float ParamValue, const TArray<float>& Tiers)
	{
		TestEqual(TEXT("증강 태그"), Augment.AugmentTag, Tag);
		TestFalse(TEXT("이름 있음"), Augment.DisplayName.IsEmpty());
		TestFalse(TEXT("설명 있음"), Augment.Description.IsEmpty());
		float Value = 0.f;
		TestTrue(TEXT("Params 항목"), Augment.FindParam(Param, Value));
		TestEqual(TEXT("Params 값"), Value, ParamValue);
		if (TestEqual(TEXT("3단계"), Augment.TierMagnitudes.Num(), Tiers.Num()))
		{
			for (int32 i = 0; i < Tiers.Num(); ++i)
			{
				TestEqual(*FString::Printf(TEXT("%d단계 값"), i + 1), Augment.TierMagnitudes[i], Tiers[i]);
			}
		}
	};
	CheckAugment(Augments[0], ArcherTag::TAG_Augment_Bow_PiercingShot_Power, SkillParamTag::TAG_SkillParam_Damage, 1.15f, { 1.15f, 1.30f, 1.50f });
	CheckAugment(Augments[1], ArcherTag::TAG_Augment_Bow_PiercingShot_Rapid, SkillParamTag::TAG_SkillParam_Cooldown, 0.85f, { 0.85f, 0.75f, 0.65f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowHunterMarkDefaultsTest, "HeavenBreach.Bow.HunterMarkDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowHunterMarkDefaultsTest::RunTest(const FString&)
{
	const UGA_BowHunterMark* CDO = GetDefault<UGA_BowHunterMark>();

	TestEqual(TEXT("쿨다운 12초"), CDO->GetBaseCooldown(), 12.f);
	TestEqual(TEXT("마나 20"), CDO->GetBaseManaCost(), 20.f);
	TestTrue(TEXT("활 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Bow));
	TestFalse(TEXT("좌클릭 입력 안 받음"), CDO->WantsAimInput());
	CheckCostAndCooldown(*this, CDO, UGE_BowHunterMarkCooldown::StaticClass(), ArcherTag::TAG_Cooldown_TrackingMark);

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckActiveAugment(*this, Augments[0], ArcherTag::TAG_Augment_Bow_HunterMark_Deep, SkillParamTag::TAG_SkillParam_MarkBonus, 0.25f, { 0.25f, 0.30f, 0.35f });
	CheckActiveAugment(*this, Augments[1], ArcherTag::TAG_Augment_Bow_HunterMark_Wide, SkillParamTag::TAG_SkillParam_Radius, 500.f, { 500.f, 600.f, 700.f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowEvasiveShotDefaultsTest, "HeavenBreach.Bow.EvasiveShotDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowEvasiveShotDefaultsTest::RunTest(const FString&)
{
	const UGA_BowEvasiveShot* CDO = GetDefault<UGA_BowEvasiveShot>();

	TestEqual(TEXT("쿨다운 6초"), CDO->GetBaseCooldown(), 6.f);
	TestEqual(TEXT("마나 20"), CDO->GetBaseManaCost(), 20.f);
	TestTrue(TEXT("활 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Bow));
	TestFalse(TEXT("좌클릭 입력 안 받음"), CDO->WantsAimInput());
	CheckCostAndCooldown(*this, CDO, UGE_BowEvasiveCooldown::StaticClass(), ArcherTag::TAG_Cooldown_EvasiveShot);

	// 후퇴는 몽타주 루트 모션(97.5cm)을 3m 로 늘린다.
	TestEqual(TEXT("후퇴 3m"), CDO->GetRetreatDistance(), 300.f);
	TestEqual(TEXT("몽타주 후퇴 97.5cm"), CDO->GetMontageRetreatDistance(), 97.5f);
	TestEqual(TEXT("루트 모션 배율"), CDO->GetMontageRootMotionScale(), 300.f / 97.5f);

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckActiveAugment(*this, Augments[0], ArcherTag::TAG_Augment_Bow_EvasiveShot_Rapid, SkillParamTag::TAG_SkillParam_Cooldown, 0.85f, { 0.85f, 0.75f, 0.65f });
	CheckActiveAugment(*this, Augments[1], ArcherTag::TAG_Augment_Bow_EvasiveShot_Light, SkillParamTag::TAG_SkillParam_ManaCost, 0.8f, { 0.80f, 0.65f, 0.50f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowTripleShotDefaultsTest, "HeavenBreach.Bow.TripleShotDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowTripleShotDefaultsTest::RunTest(const FString&)
{
	const UGA_BowTripleShot* CDO = GetDefault<UGA_BowTripleShot>();

	TestEqual(TEXT("쿨다운 8초"), CDO->GetBaseCooldown(), 8.f);
	TestEqual(TEXT("마나 25"), CDO->GetBaseManaCost(), 25.f);
	TestTrue(TEXT("좌클릭 입력을 받는다"), CDO->WantsAimInput());
	TestEqual(TEXT("관통 없음"), CDO->GetMaxPierceCount(), 0);
	TestEqual(TEXT("3발"), CDO->GetArrowCount(), 3);
	TestEqual(TEXT("좌우 6도"), CDO->GetSpreadDegrees(), 6.f);
	TestEqual(TEXT("발당 계수 0.8"), CDO->GetDamageCoef(), 0.8f);
	TestFalse(TEXT("관통 기류 안 켬"), CDO->ShowsPiercingCharge());
	TestTrue(TEXT("활 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Bow));

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_BowTripleShotCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(ArcherTag::TAG_Cooldown_TripleShot));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		TestTrue(TEXT("위력 증강"), Augments[0].AugmentTag == ArcherTag::TAG_Augment_Bow_TripleShot_Power.GetTag());
		TestTrue(TEXT("신속 증강"), Augments[1].AugmentTag == ArcherTag::TAG_Augment_Bow_TripleShot_Rapid.GetTag());
		TestTrue(TEXT("위력 단계"), Augments[0].TierMagnitudes == TArray<float>({ 1.15f, 1.30f, 1.50f }));
		TestTrue(TEXT("신속 단계"), Augments[1].TierMagnitudes == TArray<float>({ 0.85f, 0.75f, 0.65f }));
	}

	// 관통 사격 기본값은 그대로(트리플이 부모 생성자 값을 바꿔도 관통에는 영향이 없어야 한다).
	TestFalse(TEXT("관통 사격 증강과 다름"), Augments.Num() > 0 && Augments[0].AugmentTag == ArcherTag::TAG_Augment_Bow_PiercingShot_Power);
	TestTrue(TEXT("관통 사격은 관통 기류 켬"), GetDefault<UGA_BowPiercingShot>()->ShowsPiercingCharge());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBowSpreadYawTest, "HeavenBreach.Bow.SpreadYaw", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBowSpreadYawTest::RunTest(const FString&)
{
	// 가운데 화살이 조준점, 나머지는 좌우로 같은 간격.
	TestTrue(TEXT("3발 6도"), HBBowShot::SpreadYawOffsets(3, 6.f) == TArray<float>({ -6.f, 0.f, 6.f }));
	TestTrue(TEXT("1발은 가운데"), HBBowShot::SpreadYawOffsets(1, 6.f) == TArray<float>({ 0.f }));
	TestTrue(TEXT("5발 6도"), HBBowShot::SpreadYawOffsets(5, 6.f) == TArray<float>({ -12.f, -6.f, 0.f, 6.f, 12.f }));
	TestEqual(TEXT("0발은 없음"), HBBowShot::SpreadYawOffsets(0, 6.f).Num(), 0);
	return true;
}

#endif
