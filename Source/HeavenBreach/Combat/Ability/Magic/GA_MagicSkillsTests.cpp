#include "Combat/Ability/Magic/GA_MagicElementalOrb.h"
#include "Combat/Ability/Magic/GA_MagicFrostField.h"
#include "Combat/Ability/Magic/GA_MagicManaExplosion.h"
#include "Combat/Ability/Magic/GA_MagicTeleport.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Combat/Ability/Passive/GA_PassiveFocus.h"
#include "Combat/Ability/Passive/GA_PassiveGlassCannon.h"
#include "Combat/Ability/Passive/GA_PassiveManaCycle.h"
#include "Combat/Ability/Passive/GA_PassiveResidualMana.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 액티브 스킬 증강 1개를 검사한다. 이후 마법 스킬 테스트가 같이 쓴다(활 테스트 헬퍼와 같은 내용이지만, 유니티 빌드에서 이름이 충돌하지 않게 이름을 바꿨다).
	[[maybe_unused]] void CheckMagicActiveAugment(FAutomationTestBase& Test, const FHBSkillAugment& Augment, const FGameplayTag Tag, const FGameplayTag Param, const float ParamValue, const TArray<float>& Tiers)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicPassivesTest, "HeavenBreach.Magic.Passives", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicPassivesTest::RunTest(const FString&)
{
	TestEqual(TEXT("집중 GE"), GetDefault<UGA_PassiveFocus>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_FocusPassive::StaticClass()));
	TestEqual(TEXT("마력 순환 GE"), GetDefault<UGA_PassiveManaCycle>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_ManaCyclePassive::StaticClass()));
	TestEqual(TEXT("잔류 마력 GE"), GetDefault<UGA_PassiveResidualMana>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_ResidualManaPassive::StaticClass()));
	TestEqual(TEXT("유리 대포 GE"), GetDefault<UGA_PassiveGlassCannon>()->GetSelfPassiveEffect(), TSubclassOf<UGameplayEffect>(UGE_GlassCannonPassive::StaticClass()));
	const TArray<FHBSkillAugment>& Focus = GetDefault<UGA_PassiveFocus>()->GetAugments();
	TestEqual(TEXT("집중 증강 1개"), Focus.Num(), 1);
	TestEqual(TEXT("집중 증강 태그"), Focus[0].AugmentTag, MagicTag::TAG_Augment_Passive_Focus_Boost.GetTag());
	TestEqual(TEXT("집중 단계"), Focus[0].TierMagnitudes, TArray<float>({ 0.65f, 0.55f, 0.45f }));
	const TArray<FHBSkillAugment>& Cycle = GetDefault<UGA_PassiveManaCycle>()->GetAugments();
	TestEqual(TEXT("마력 순환 증강 태그"), Cycle[0].AugmentTag, MagicTag::TAG_Augment_Passive_ManaCycle_Boost.GetTag());
	TestEqual(TEXT("마력 순환 단계(최종 배율)"), Cycle[0].TierMagnitudes, TArray<float>({ 0.70f, 0.60f, 0.50f }));
	TestTrue(TEXT("마력 순환 증강 GE 없음(코드가 단계를 읽는다)"), Cycle[0].GrantEffect == nullptr);
	// 마력 순환은 태그만 준다. 전역 마나 배율을 건드리면 달리기·방어까지 줄어든다(설계 2.1: 액티브만).
	TestEqual(TEXT("마력 순환 GE 모디파이어 없음"), GetDefault<UGE_ManaCyclePassive>()->Modifiers.Num(), 0);
	TestTrue(TEXT("마력 순환 상태 태그"), GetDefault<UGE_ManaCyclePassive>()->GetGrantedTags().HasTagExact(MagicTag::TAG_State_ManaCycle));
	const TArray<FHBSkillAugment>& Glass = GetDefault<UGA_PassiveGlassCannon>()->GetAugments();
	TestEqual(TEXT("유리 대포 단계(추가분)"), Glass[0].TierMagnitudes, TArray<float>({ 0.10f, 0.20f, 0.35f }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicCooldownEffectsTest, "HeavenBreach.Magic.CooldownEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicCooldownEffectsTest::RunTest(const FString&)
{
	TestTrue(TEXT("구체 쿨다운 태그"), GetDefault<UGE_MagicOrbCooldown>()->GetGrantedTags().HasTagExact(MagicTag::TAG_Cooldown_ElementalOrb));
	TestTrue(TEXT("순간이동 쿨다운 태그"), GetDefault<UGE_MagicTeleportCooldown>()->GetGrantedTags().HasTagExact(MagicTag::TAG_Cooldown_Teleport));
	TestTrue(TEXT("서리 쿨다운 태그"), GetDefault<UGE_MagicFrostCooldown>()->GetGrantedTags().HasTagExact(MagicTag::TAG_Cooldown_FrostField));
	TestTrue(TEXT("폭발 쿨다운 태그"), GetDefault<UGE_MagicExplosionCooldown>()->GetGrantedTags().HasTagExact(MagicTag::TAG_Cooldown_ManaExplosion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicCastTimingTest, "HeavenBreach.Magic.CastTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicCastTimingTest::RunTest(const FString&)
{
	// 발동 시각 = ReleaseTime / MontagePlayRate × 캐스팅 배율. 몽타주 재생 속도 = MontagePlayRate / 캐스팅 배율. 두 값이 같은 프레임을 가리켜야 한다.
	TestEqual(TEXT("구체 기본"), HBMagic::CastReleaseDelay(0.4f, 1.f, 1.f), 0.4f);
	TestEqual(TEXT("순간이동 2배속"), HBMagic::CastReleaseDelay(0.3f, 2.f, 1.f), 0.15f);
	TestEqual(TEXT("폭발 + 집중"), HBMagic::CastReleaseDelay(1.6f, 1.f, 0.75f), 1.2f);
	TestEqual(TEXT("구체 + 집중 3단계"), HBMagic::CastReleaseDelay(0.4f, 1.f, 0.45f), 0.18f);
	TestTrue(TEXT("재생 속도 0 방어"), HBMagic::CastReleaseDelay(0.4f, 0.f, 1.f) > 0.f);
	TestEqual(TEXT("재생 속도 기본"), HBMagic::CastMontagePlayRate(1.f, 1.f), 1.f);
	TestEqual(TEXT("재생 속도 순간이동"), HBMagic::CastMontagePlayRate(2.f, 1.f), 2.f);
	TestEqual(TEXT("재생 속도 집중"), HBMagic::CastMontagePlayRate(1.f, 0.75f), 1.f / 0.75f);
	// 몽타주 시각 ReleaseTime 이 실제로 재생되는 시각과 발동 타이머가 일치하는가
	TestEqual(TEXT("몽타주와 타이머 일치"), 1.6f / HBMagic::CastMontagePlayRate(1.f, 0.55f), HBMagic::CastReleaseDelay(1.6f, 1.f, 0.55f), KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicElementalOrbDefaultsTest, "HeavenBreach.Magic.ElementalOrbDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicElementalOrbDefaultsTest::RunTest(const FString&)
{
	const UGA_MagicElementalOrb* CDO = GetDefault<UGA_MagicElementalOrb>();
	TestEqual(TEXT("마나 20"), CDO->GetBaseManaCost(), 20.f);
	TestEqual(TEXT("쿨다운 6초"), CDO->GetBaseCooldown(), 6.f);
	TestTrue(TEXT("지팡이 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Staff));
	TestTrue(TEXT("캐스팅 스킬"), CDO->IsCastingSkill());
	TestEqual(TEXT("발동 0.4초"), CDO->GetReleaseTime(), 0.4f);
	TestEqual(TEXT("손 이펙트 종류 구체"), CDO->GetCastHandFxIndex(), 1);
	TestEqual(TEXT("손 이펙트 크기"), CDO->GetCastHandScale(), 0.2f);
	TestEqual(TEXT("속도 1800"), CDO->GetOrbSpeed(), 1800.f);
	TestEqual(TEXT("피해 계수 1.6"), CDO->GetDamageCoef(), 1.6f);

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_MagicOrbCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(MagicTag::TAG_Cooldown_ElementalOrb));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckMagicActiveAugment(*this, Augments[0], MagicTag::TAG_Augment_Magic_ElementalOrb_Power, SkillParamTag::TAG_SkillParam_Damage, 1.15f, { 1.15f, 1.30f, 1.50f });
	CheckMagicActiveAugment(*this, Augments[1], MagicTag::TAG_Augment_Magic_ElementalOrb_Rapid, SkillParamTag::TAG_SkillParam_Cooldown, 0.85f, { 0.85f, 0.75f, 0.65f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicTeleportDefaultsTest, "HeavenBreach.Magic.TeleportDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicTeleportDefaultsTest::RunTest(const FString&)
{
	const UGA_MagicTeleport* CDO = GetDefault<UGA_MagicTeleport>();
	TestEqual(TEXT("스태미나 20"), CDO->GetBaseManaCost(), 20.f);
	TestEqual(TEXT("쿨다운 8초"), CDO->GetBaseCooldown(), 8.f);
	TestTrue(TEXT("지팡이 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Staff));
	TestFalse(TEXT("캐스팅 스킬 아님"), CDO->IsCastingSkill());
	TestEqual(TEXT("발동 0.6"), CDO->GetReleaseTime(), 0.6f);
	TestEqual(TEXT("재생 속도 2"), CDO->GetMontagePlayRate(), 2.f);
	TestEqual(TEXT("실제 지연 0.3초"), HBMagic::CastReleaseDelay(CDO->GetReleaseTime(), CDO->GetMontagePlayRate(), 1.f), 0.3f);
	TestEqual(TEXT("손 이펙트 종류 순간이동"), CDO->GetCastHandFxIndex(), 2);
	TestEqual(TEXT("손 이펙트 크기"), CDO->GetCastHandScale(), 0.16f);
	TestEqual(TEXT("거리 400"), CDO->GetDistance(), 400.f);
	TestEqual(TEXT("벽 여유 12"), CDO->GetWallMargin(), 12.f);
	TestEqual(TEXT("지면 법선 0.7"), CDO->GetMinGroundNormalZ(), 0.7f);
	TestEqual(TEXT("높이차 100"), CDO->GetMaxHeightDelta(), 100.f);
	TestEqual(TEXT("최소 이동 40"), CDO->GetMinMove(), 40.f);
	TestEqual(TEXT("지면 띄움 2"), CDO->GetGroundLift(), 2.f);

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_MagicTeleportCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(MagicTag::TAG_Cooldown_Teleport));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckMagicActiveAugment(*this, Augments[0], MagicTag::TAG_Augment_Magic_Teleport_Far, SkillParamTag::TAG_SkillParam_Distance, 500.f, { 500.f, 600.f, 700.f });
	CheckMagicActiveAugment(*this, Augments[1], MagicTag::TAG_Augment_Magic_Teleport_Rapid, SkillParamTag::TAG_SkillParam_Cooldown, 0.85f, { 0.85f, 0.75f, 0.65f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicTeleportValidationTest, "HeavenBreach.Magic.TeleportValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicTeleportValidationTest::RunTest(const FString&)
{
	// (법선 Z, 높이차, 이동 거리) 기준값: 0.7 / 100 / 40
	TestTrue(TEXT("평지"), HBMagic::IsTeleportDestinationValid(1.f, 0.f, 400.f, 0.7f, 100.f, 40.f));
	TestFalse(TEXT("가파른 경사"), HBMagic::IsTeleportDestinationValid(0.5f, 0.f, 400.f, 0.7f, 100.f, 40.f));
	TestTrue(TEXT("법선 경계"), HBMagic::IsTeleportDestinationValid(0.7f, 0.f, 400.f, 0.7f, 100.f, 40.f));
	TestTrue(TEXT("높이차 경계"), HBMagic::IsTeleportDestinationValid(1.f, 100.f, 400.f, 0.7f, 100.f, 40.f));
	TestFalse(TEXT("내리막 150 초과"), HBMagic::IsTeleportDestinationValid(1.f, -150.f, 400.f, 0.7f, 100.f, 40.f));
	TestFalse(TEXT("높이차 초과"), HBMagic::IsTeleportDestinationValid(1.f, 101.f, 400.f, 0.7f, 100.f, 40.f));
	TestTrue(TEXT("내리막 80"), HBMagic::IsTeleportDestinationValid(1.f, -80.f, 400.f, 0.7f, 100.f, 40.f));
	TestFalse(TEXT("이동 거리 경계"), HBMagic::IsTeleportDestinationValid(1.f, 0.f, 40.f, 0.7f, 100.f, 40.f));
	TestTrue(TEXT("이동 거리 초과"), HBMagic::IsTeleportDestinationValid(1.f, 0.f, 41.f, 0.7f, 100.f, 40.f));
	// 경사 띄움 = 반지름 × (1/법선Z − 1) + 여유. 평지는 여유만.
	TestEqual(TEXT("평지 띄움"), HBMagic::TeleportSlopeLift(34.f, 1.f, 2.f), 2.f);
	TestEqual(TEXT("30도 띄움"), HBMagic::TeleportSlopeLift(34.f, 0.8660254f, 2.f), 34.f * (1.f / 0.8660254f - 1.f) + 2.f, 0.01f);
	TestTrue(TEXT("법선 0 방어"), FMath::IsFinite(HBMagic::TeleportSlopeLift(34.f, 0.f, 2.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicFrostFieldDefaultsTest, "HeavenBreach.Magic.FrostFieldDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicFrostFieldDefaultsTest::RunTest(const FString&)
{
	const UGA_MagicFrostField* CDO = GetDefault<UGA_MagicFrostField>();
	TestEqual(TEXT("스태미나 25"), CDO->GetBaseManaCost(), 25.f);
	TestEqual(TEXT("쿨다운 12초"), CDO->GetBaseCooldown(), 12.f);
	TestTrue(TEXT("지팡이 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Staff));
	TestFalse(TEXT("캐스팅 스킬 아님"), CDO->IsCastingSkill());
	TestEqual(TEXT("발동 0.35"), CDO->GetReleaseTime(), 0.35f);
	TestEqual(TEXT("손 이펙트 종류 서리"), CDO->GetCastHandFxIndex(), 3);
	TestEqual(TEXT("손 이펙트 크기"), CDO->GetCastHandScale(), 0.16f);
	TestEqual(TEXT("반경 300"), CDO->GetRadius(), 300.f);
	TestEqual(TEXT("지속 4초"), CDO->GetDuration(), 4.f);
	TestEqual(TEXT("감속 0.4"), CDO->GetSlow(), 0.4f);
	TestEqual(TEXT("조준 거리 1200"), CDO->GetAimTraceDistance(), 1200.f);

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_MagicFrostCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(MagicTag::TAG_Cooldown_FrostField));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckMagicActiveAugment(*this, Augments[0], MagicTag::TAG_Augment_Magic_FrostField_Bitter, SkillParamTag::TAG_SkillParam_Slow, 0.50f, { 0.50f, 0.55f, 0.60f });
	CheckMagicActiveAugment(*this, Augments[1], MagicTag::TAG_Augment_Magic_FrostField_Wide, SkillParamTag::TAG_SkillParam_Radius, 350.f, { 350.f, 400.f, 450.f });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMagicManaExplosionDefaultsTest, "HeavenBreach.Magic.ManaExplosionDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMagicManaExplosionDefaultsTest::RunTest(const FString&)
{
	const UGA_MagicManaExplosion* CDO = GetDefault<UGA_MagicManaExplosion>();
	TestEqual(TEXT("스태미나 35"), CDO->GetBaseManaCost(), 35.f);
	TestEqual(TEXT("쿨다운 15초"), CDO->GetBaseCooldown(), 15.f);
	TestTrue(TEXT("지팡이 필요"), CDO->GetRequiredWeaponTags().HasTagExact(ItemTag::TAG_Weapon_Staff));
	TestTrue(TEXT("캐스팅 스킬"), CDO->IsCastingSkill());
	TestEqual(TEXT("발동 1.6"), CDO->GetReleaseTime(), 1.6f);
	TestEqual(TEXT("손 이펙트 종류 폭발"), CDO->GetCastHandFxIndex(), 4);
	TestEqual(TEXT("손 이펙트 크기"), CDO->GetCastHandScale(), 0.14f);
	TestEqual(TEXT("반경 300"), CDO->GetRadius(), 300.f);
	TestEqual(TEXT("피해 계수 2.5"), CDO->GetDamageCoef(), 2.5f);

	const UGameplayEffect* CooldownGE = CDO->GetCooldownGameplayEffect();
	TestTrue(TEXT("쿨다운 GE 클래스"), CooldownGE && CooldownGE->GetClass() == UGE_MagicExplosionCooldown::StaticClass());
	const FGameplayTagContainer* CooldownTags = CDO->GetCooldownTags();
	TestTrue(TEXT("쿨다운 태그"), CooldownTags && CooldownTags->HasTagExact(MagicTag::TAG_Cooldown_ManaExplosion));

	const TArray<FHBSkillAugment>& Augments = CDO->GetAugments();
	if (!TestEqual(TEXT("증강 2개"), Augments.Num(), 2))
	{
		return true;
	}
	CheckMagicActiveAugment(*this, Augments[0], MagicTag::TAG_Augment_Magic_ManaExplosion_Power, SkillParamTag::TAG_SkillParam_Damage, 1.15f, { 1.15f, 1.30f, 1.50f });
	CheckMagicActiveAugment(*this, Augments[1], MagicTag::TAG_Augment_Magic_ManaExplosion_Wide, SkillParamTag::TAG_SkillParam_Radius, 350.f, { 350.f, 400.f, 450.f });
	return true;
}

#endif
