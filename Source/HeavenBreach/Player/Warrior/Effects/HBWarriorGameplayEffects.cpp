#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"

#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

namespace
{
	void AddModifier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute,
		const EGameplayModOp::Type Operation, const float Magnitude)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	}
}

UGE_WarriorTaggedDuration::UGE_WarriorTaggedDuration()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
}

UGE_WarriorBattleCryBuff::UGE_WarriorBattleCryBuff()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute(),
		EGameplayModOp::Multiplicitive, 1.2f);
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(),
		EGameplayModOp::Additive, 1.f);
}

UGE_WarriorCombatInstinctStack::UGE_WarriorCombatInstinctStack()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.f));
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute(),
		EGameplayModOp::Additive, 0.05f);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 5;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
}

UGE_WarriorKillingMomentumBuff::UGE_WarriorKillingMomentumBuff()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute(),
		EGameplayModOp::Additive, 0.2f);
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(),
		EGameplayModOp::Additive, 0.3f);
}

UGE_WarriorStrongBodyPassive::UGE_WarriorStrongBodyPassive()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute(),
		EGameplayModOp::Multiplicitive, 1.2f);
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_HPAttribute(),
		EGameplayModOp::Multiplicitive, 1.2f);
}
