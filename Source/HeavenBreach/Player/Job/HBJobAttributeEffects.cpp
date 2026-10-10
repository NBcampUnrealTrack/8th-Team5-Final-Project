#include "Player/Job/HBJobAttributeEffects.h"

#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/Job/HBJobData.h"

namespace
{
	void AddOverride(UGameplayEffect& Effect, const FGameplayAttribute& Attribute, const float Value)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
	}

	void Configure(UGameplayEffect& Effect, const float HP, const float Mana,
		const float AttackPower, const float AttackSpeed, const float Defense,
		const float MoveSpeed)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::Instant;
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute(), HP);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_HPAttribute(), HP);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute(), Mana);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), Mana);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute(), AttackPower);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute(), AttackSpeed);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute(), Defense);
		AddOverride(Effect, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(), MoveSpeed);
	}
}

bool HBJobAttributes::GetJobBaseAttribute(const UHBJobData* Job, const FGameplayAttribute& Attribute, float& OutValue)
{
	if (!Job || !Attribute.IsValid())
	{
		return false;
	}

	// 캐릭터가 능력치를 줄 때와 같은 순서로 고른다: 직업 DA 의 GE, 없으면 직업별 임시 GE.
	TSubclassOf<UGameplayEffect> EffectClass = Job->DefaultAttributesEffect;
	if (!EffectClass)
	{
		if (Job->JobId == TEXT("Warrior")) EffectClass = UGE_DefaultAttributes_Warrior::StaticClass();
		else if (Job->JobId == TEXT("Archer")) EffectClass = UGE_DefaultAttributes_Archer::StaticClass();
		else if (Job->JobId == TEXT("Mage")) EffectClass = UGE_DefaultAttributes_Mage::StaticClass();
	}

	const UGameplayEffect* Effect = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!Effect)
	{
		return false;
	}

	// 능력치를 "정하는" Override 모디파이어만 본다. 더하기 곱하기는 기본값 위에 얹는 것이라 여기서는 다루지 않는다.
	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		if (Modifier.Attribute != Attribute || Modifier.ModifierOp != EGameplayModOp::Override)
		{
			continue;
		}

		float Magnitude = 0.f;
		if (Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Magnitude))
		{
			OutValue = Magnitude;
			return true;
		}
	}

	return false;
}

UGE_DefaultAttributes_Warrior::UGE_DefaultAttributes_Warrior()
{
	Configure(*this, 130.f, 100.f, 5.f, 1.f, 8.f, 1.f);
}

UGE_DefaultAttributes_Archer::UGE_DefaultAttributes_Archer()
{
	Configure(*this, 90.f, 100.f, 5.f, 1.2f, 3.f, 1.1f);
}

UGE_DefaultAttributes_Mage::UGE_DefaultAttributes_Mage()
{
	Configure(*this, 80.f, 150.f, 5.f, 1.f, 2.f, 1.f);
}
