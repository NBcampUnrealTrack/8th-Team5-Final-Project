#include "AI/EnemyCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Ability/HBSkillTargets.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/AttributeSet/HBAttributeRules.h"
#include "Player/HBPlayerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// GE 에 Attribute 를 Op 로 Value 만큼 바꾸는 고정 모디파이어가 있는가.
	bool RulesHasModifier(const UGameplayEffect* Effect, const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, float Value)
	{
		for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
		{
			float Magnitude = 0.f;
			if (Modifier.Attribute == Attribute && Modifier.ModifierOp == Op
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Magnitude))
			{
				return FMath::IsNearlyEqual(Magnitude, Value);
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBMaxHealthRaiseTest, "HeavenBreach.Player.MaxHealthRaise", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBMaxHealthRaiseTest::RunTest(const FString&)
{
	// 최대 체력이 지금까지의 최고치를 넘어 오르면 넘은 만큼 현재 체력도 채운다. 내리면 넘치는 만큼만 자른다.
	float Highest = 100.f;
	TestEqual(TEXT("증강 +20 → 현재도 +20"), HBAttributeRules::HealthOnMaxChanged(80.f, 100.f, 120.f, Highest), 100.f);
	TestEqual(TEXT("최고치 120 기록"), Highest, 120.f);

	TestEqual(TEXT("장비 벗음 120→100: 넘치는 만큼만"), HBAttributeRules::HealthOnMaxChanged(110.f, 120.f, 100.f, Highest), 100.f);
	TestEqual(TEXT("장비 다시 낌 100→120: 최고치 이하라 안 채움"), HBAttributeRules::HealthOnMaxChanged(100.f, 100.f, 120.f, Highest), 100.f);
	TestEqual(TEXT("최고치 그대로 120"), Highest, 120.f);

	TestEqual(TEXT("최고치를 일부만 넘으면 넘은 만큼만 100→130"), HBAttributeRules::HealthOnMaxChanged(100.f, 100.f, 130.f, Highest), 110.f);
	TestEqual(TEXT("최고치 130"), Highest, 130.f);

	TestEqual(TEXT("죽은 상태는 그대로 0"), HBAttributeRules::HealthOnMaxChanged(0.f, 130.f, 200.f, Highest), 0.f);
	TestEqual(TEXT("최대치를 넘게 채우지 않음"), HBAttributeRules::HealthOnMaxChanged(199.f, 200.f, 300.f, Highest), 299.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBWeaponBonusTest, "HeavenBreach.Player.WeaponBonus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBWeaponBonusTest::RunTest(const FString&)
{
	// 검(단검 포함): 방어 +5, 최대 체력 +20. 활: 이동 속도 +10%. 지팡이·맨손: 없음(지팡이는 마법 피해 1.5배가 따로 있다).
	TestTrue(TEXT("검 → 검 보너스"), HBWeaponBonus::EffectFor(ItemTag::TAG_Weapon_Sword) == UGE_WeaponBonusSword::StaticClass());
	TestTrue(TEXT("단검 → 검 보너스"), HBWeaponBonus::EffectFor(ItemTag::TAG_Weapon_Dagger) == UGE_WeaponBonusSword::StaticClass());
	TestTrue(TEXT("활 → 활 보너스"), HBWeaponBonus::EffectFor(ItemTag::TAG_Weapon_Bow) == UGE_WeaponBonusBow::StaticClass());
	TestTrue(TEXT("지팡이 → 없음"), HBWeaponBonus::EffectFor(ItemTag::TAG_Weapon_Staff) == nullptr);
	TestTrue(TEXT("맨손 → 없음"), HBWeaponBonus::EffectFor(ItemTag::TAG_Weapon_Unarmed) == nullptr);

	const UGameplayEffect* Sword = GetDefault<UGE_WeaponBonusSword>();
	TestEqual(TEXT("검 무한"), Sword->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TestTrue(TEXT("검 방어 +5"), RulesHasModifier(Sword, UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute(), EGameplayModOp::Additive, 5.f));
	TestTrue(TEXT("검 최대 체력 +20"), RulesHasModifier(Sword, UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute(), EGameplayModOp::Additive, 20.f));

	const UGameplayEffect* Bow = GetDefault<UGE_WeaponBonusBow>();
	TestEqual(TEXT("활 무한"), Bow->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TestTrue(TEXT("활 이속 +10%"), RulesHasModifier(Bow, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(), EGameplayModOp::Additive, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBManaRegenDefaultTest, "HeavenBreach.Player.ManaRegenDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBManaRegenDefaultTest::RunTest(const FString&)
{
	// 마나 자연 회복 초당 0.5(10-14 조정, 전에는 2).
	const FFloatProperty* Prop = FindFProperty<FFloatProperty>(AHBPlayerCharacter::StaticClass(), TEXT("ManaRegenPerSecond"));
	if (TestNotNull(TEXT("ManaRegenPerSecond 속성"), Prop))
	{
		TestEqual(TEXT("초당 0.5"), Prop->GetPropertyValue_InContainer(GetDefault<AHBPlayerCharacter>()), 0.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSkillTargetsTest, "HeavenBreach.Player.SkillTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSkillTargetsTest::RunTest(const FString&)
{
	// 범위 스킬 대상: 반경 안의 피해 받을 수 있는 액터. 시전자·시체는 뺀다.
	UClass* EnemyClass = LoadClass<AActor>(nullptr, TEXT("/Game/Monsters/Remnant_Blade/Blueprint/BP_Remnant_Blade.BP_Remnant_Blade_C"));
	if (!TestNotNull(TEXT("몬스터 클래스"), EnemyClass))
	{
		return true;
	}

	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Caster = World->SpawnActor<AActor>(EnemyClass, FTransform(FVector(0.f, 0.f, 0.f)), Params);
	AActor* Near = World->SpawnActor<AActor>(EnemyClass, FTransform(FVector(150.f, 0.f, 0.f)), Params);
	AActor* Dead = World->SpawnActor<AActor>(EnemyClass, FTransform(FVector(-150.f, 0.f, 0.f)), Params);
	AActor* Far = World->SpawnActor<AActor>(EnemyClass, FTransform(FVector(2000.f, 0.f, 0.f)), Params);
	if (UAbilitySystemComponent* DeadASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Dead))
	{
		DeadASC->AddLooseGameplayTag(AbilityTag::TAG_State_Dead);
	}

	TArray<AActor*> Targets;
	HBSkillTargets::FindInSphere(World, FVector::ZeroVector, 400.f, Caster, Targets);
	TestTrue(TEXT("가까운 대상 포함"), Targets.Contains(Near));
	TestFalse(TEXT("시전자 제외"), Targets.Contains(Caster));
	TestFalse(TEXT("시체 제외"), Targets.Contains(Dead));
	TestFalse(TEXT("반경 밖 제외"), Targets.Contains(Far));
	TestEqual(TEXT("한 번씩만"), Targets.Num(), 1);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
