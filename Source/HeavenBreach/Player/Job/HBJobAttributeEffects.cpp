#include "Player/Job/HBJobAttributeEffects.h"

#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

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

UGE_DefaultAttributes_Warrior::UGE_DefaultAttributes_Warrior()
{
	Configure(*this, 130.f, 100.f, 13.f, 1.f, 8.f, 1.f);
}

UGE_DefaultAttributes_Archer::UGE_DefaultAttributes_Archer()
{
	Configure(*this, 90.f, 100.f, 11.f, 1.2f, 3.f, 1.1f);
}

UGE_DefaultAttributes_Mage::UGE_DefaultAttributes_Mage()
{
	Configure(*this, 80.f, 150.f, 15.f, 1.f, 2.f, 1.f);
}
