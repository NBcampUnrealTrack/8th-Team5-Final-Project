#include "Player/Archer/Effects/HBArcherGameplayEffects.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

namespace
{
	void Configure(UGameplayEffect& Effect, const float Duration,
		const FGameplayAttribute& Attribute, const EGameplayModOp::Type Op, const float Value)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
		Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Op;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
	}
}
UGE_ArcherSwiftMovement::UGE_ArcherSwiftMovement()
{ Configure(*this, 3.f, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(), EGameplayModOp::Additive, 0.5f); }
UGE_ArcherRapidFire::UGE_ArcherRapidFire()
{ Configure(*this, 4.f, UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute(), EGameplayModOp::Additive, 0.5f); }
UGE_ArcherTacticalRetreat::UGE_ArcherTacticalRetreat()
{ Configure(*this, 3.f, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(), EGameplayModOp::Additive, 0.3f); }
