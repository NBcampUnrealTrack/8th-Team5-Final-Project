#include "Player/HBStamina.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/Job/HBJobAttributeEffects.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 직업 기본 GE 에 Attribute 를 Value 로 Override 하는 모디파이어가 있는가.
	bool StaminaHasOverride(const UGameplayEffect* Effect, const FGameplayAttribute& Attribute, float Value)
	{
		for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
		{
			float Magnitude = 0.f;
			if (Modifier.Attribute == Attribute && Modifier.ModifierOp == EGameplayModOp::Override
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Magnitude))
			{
				return FMath::IsNearlyEqual(Magnitude, Value);
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBStaminaSprintCostTest, "HeavenBreach.Stamina.SprintCost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBStaminaSprintCostTest::RunTest(const FString&)
{
	TestEqual(TEXT("기본 2.5"), HBStamina::SprintCostPerSecond(2.5f, 1.f), 2.5f);
	TestEqual(TEXT("사냥꾼의 호흡 -30%"), HBStamina::SprintCostPerSecond(2.5f, 0.7f), 1.75f);
	TestEqual(TEXT("전투 호흡 = 0"), HBStamina::SprintCostPerSecond(2.5f, 0.f), 0.f);
	TestEqual(TEXT("음수 배율은 0"), HBStamina::SprintCostPerSecond(2.5f, -1.f), 0.f);
	TestEqual(TEXT("방어 기본 4"), HBStamina::BlockCostPerSecond(4.f, 1.f), 4.f);
	TestEqual(TEXT("견고한 자세 0.7"), HBStamina::BlockCostPerSecond(4.f, 0.7f), 2.8f);
	TestEqual(TEXT("남은 만큼만 깎음"), HBStamina::SpendAmount(0.3f, 4.f, 0.25f), 0.3f);
	TestEqual(TEXT("한 틱 소모"), HBStamina::SpendAmount(50.f, 4.f, 0.25f), 1.f);
	TestEqual(TEXT("음수 비용은 0"), HBStamina::SpendAmount(50.f, -4.f, 0.25f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBStaminaRegenTest, "HeavenBreach.Stamina.Regen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBStaminaRegenTest::RunTest(const FString&)
{
	// 스태미나: 마지막 소모 뒤 2초가 지나야 찬다.
	TestEqual(TEXT("대기 중엔 0"), HBStamina::StaminaRegenAmount(50.f, 100.f, 10.f, 1.f, 0.25f, 1.9, 2.f), 0.f);
	TestEqual(TEXT("대기 끝나면 한 틱 2.5"), HBStamina::StaminaRegenAmount(50.f, 100.f, 10.f, 1.f, 0.25f, 2.0, 2.f), 2.5f);
	TestEqual(TEXT("사냥꾼의 호흡 +20%"), HBStamina::StaminaRegenAmount(50.f, 100.f, 10.f, 1.2f, 0.25f, 5.0, 2.f), 3.f);
	TestEqual(TEXT("최대치에서 자름"), HBStamina::StaminaRegenAmount(99.f, 100.f, 10.f, 1.f, 0.25f, 5.0, 2.f), 1.f);
	TestEqual(TEXT("가득 차면 0"), HBStamina::StaminaRegenAmount(100.f, 100.f, 10.f, 1.f, 0.25f, 5.0, 2.f), 0.f);
	TestEqual(TEXT("음수 배율은 0"), HBStamina::StaminaRegenAmount(50.f, 100.f, 10.f, -1.f, 0.25f, 5.0, 2.f), 0.f);

	// 마나: 대기 없이 항상 초당 2. 장비 회복(Flat)·배율이 붙는다.
	TestEqual(TEXT("마나 한 틱 0.5"), HBStamina::ManaRegenAmount(10.f, 100.f, 2.f, 0.f, 1.f, 0.25f), 0.5f);
	TestEqual(TEXT("장비 +2"), HBStamina::ManaRegenAmount(10.f, 100.f, 2.f, 2.f, 1.f, 0.25f), 1.f);
	TestEqual(TEXT("마나 최대치에서 자름"), HBStamina::ManaRegenAmount(99.8f, 100.f, 2.f, 0.f, 1.f, 0.25f), 0.2f, 0.001f);
	TestEqual(TEXT("마나 음수 회복은 0"), HBStamina::ManaRegenAmount(10.f, 100.f, 2.f, -5.f, 1.f, 0.25f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBStaminaAttributeDefaultsTest, "HeavenBreach.Stamina.AttributeDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBStaminaAttributeDefaultsTest::RunTest(const FString&)
{
	const UHB_PlayerAttributeSet* Set = GetDefault<UHB_PlayerAttributeSet>();
	TestEqual(TEXT("스태미나 100"), Set->GetPlayer_Stamina(), 100.f);
	TestEqual(TEXT("최대 스태미나 100"), Set->GetPlayer_MaxStamina(), 100.f);
	TestEqual(TEXT("스태미나 회복 배율 1"), Set->GetPlayer_StaminaRegenMultiplier(), 1.f);
	TestEqual(TEXT("달리기 스태미나 배율 1"), Set->GetPlayer_SprintStaminaCostMultiplier(), 1.f);

	// 직업 기본 GE: 스태미나 100 은 모든 직업, 마나는 직업값 그대로.
	const TArray<TPair<const UGameplayEffect*, float>> Jobs = {
		{ GetDefault<UGE_DefaultAttributes_Warrior>(), 100.f },
		{ GetDefault<UGE_DefaultAttributes_Archer>(), 100.f },
		{ GetDefault<UGE_DefaultAttributes_Mage>(), 150.f } };
	for (const TPair<const UGameplayEffect*, float>& Job : Jobs)
	{
		const FString Name = GetNameSafe(Job.Key->GetClass());
		TestTrue(*(Name + TEXT(" 최대 스태미나 100")), StaminaHasOverride(Job.Key, UHB_PlayerAttributeSet::GetPlayer_MaxStaminaAttribute(), 100.f));
		TestTrue(*(Name + TEXT(" 스태미나 100")), StaminaHasOverride(Job.Key, UHB_PlayerAttributeSet::GetPlayer_StaminaAttribute(), 100.f));
		TestTrue(*(Name + TEXT(" 최대 마나 유지")), StaminaHasOverride(Job.Key, UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute(), Job.Value));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBStaminaTickTest, "HeavenBreach.Stamina.Tick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBStaminaTickTest::RunTest(const FString&)
{
	// 기본 상태: 마나 50/100, 스태미나 50/100, 0.25초 틱, 마지막 스태미나 사용 5초 전.
	auto Base = []()
	{
		HBStamina::FResourceTickInput In;
		In.Mana = 50.f; In.MaxMana = 100.f; In.ManaRegenPerSecond = 2.f;
		In.Stamina = 50.f; In.MaxStamina = 100.f; In.StaminaRegenPerSecond = 10.f; In.StaminaRegenDelay = 2.f;
		In.Interval = 0.25f; In.SecondsSinceStaminaUse = 5.0;
		return In;
	};

	{
		const HBStamina::FResourceTickResult R = HBStamina::ComputeTick(Base());
		TestEqual(TEXT("쉬는 중: 마나 +0.5"), R.ManaDelta, 0.5f);
		TestEqual(TEXT("쉬는 중: 스태미나 +2.5"), R.StaminaDelta, 2.5f);
		TestFalse(TEXT("쉬는 중: 사용 아님"), R.bStaminaUsed);
	}
	{
		HBStamina::FResourceTickInput In = Base();
		In.SprintCostPerSecond = 2.5f;
		const HBStamina::FResourceTickResult R = HBStamina::ComputeTick(In);
		TestEqual(TEXT("달리는 중에도 마나는 참"), R.ManaDelta, 0.5f);
		TestEqual(TEXT("달리기 스태미나 -0.625"), R.StaminaDelta, -0.625f);
		TestTrue(TEXT("달리기는 스태미나 사용"), R.bStaminaUsed);
		TestFalse(TEXT("아직 안 멈춤"), R.bStopStaminaActions);
	}
	{
		HBStamina::FResourceTickInput In = Base();
		In.Stamina = 0.5f; In.SprintCostPerSecond = 2.5f; In.BlockCostPerSecond = 4.f;
		const HBStamina::FResourceTickResult R = HBStamina::ComputeTick(In);
		TestEqual(TEXT("남은 만큼만 깎음"), R.StaminaDelta, -0.5f);
		TestTrue(TEXT("바닥나면 달리기·방어 멈춤"), R.bStopStaminaActions);
		TestEqual(TEXT("바닥나도 마나는 참"), R.ManaDelta, 0.5f);
	}
	{
		// 전투 호흡: 달리기 비용 0 이면 사용이 아니라 회복한다.
		HBStamina::FResourceTickInput In = Base();
		In.SprintCostPerSecond = 0.f;
		const HBStamina::FResourceTickResult R = HBStamina::ComputeTick(In);
		TestEqual(TEXT("공짜 달리기는 회복"), R.StaminaDelta, 2.5f);
		TestFalse(TEXT("공짜 달리기는 사용 아님"), R.bStaminaUsed);
	}
	{
		HBStamina::FResourceTickInput In = Base();
		In.SecondsSinceStaminaUse = 1.0;
		const HBStamina::FResourceTickResult R = HBStamina::ComputeTick(In);
		TestEqual(TEXT("대기 중엔 스태미나 그대로"), R.StaminaDelta, 0.f);
		TestEqual(TEXT("대기 중에도 마나는 참"), R.ManaDelta, 0.5f);
	}
	return true;
}

#endif
