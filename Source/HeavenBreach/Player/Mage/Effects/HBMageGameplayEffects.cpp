#include "Player/Mage/Effects/HBMageGameplayEffects.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

UGE_MageGlassCannon::UGE_MageGlassCannon()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& HP = Modifiers.AddDefaulted_GetRef();
	HP.Attribute = UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute();
	HP.ModifierOp = EGameplayModOp::Multiplicitive;
	HP.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.8f));
	FGameplayModifierInfo& Power = Modifiers.AddDefaulted_GetRef();
	Power.Attribute = UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute();
	Power.ModifierOp = EGameplayModOp::Multiplicitive;
	Power.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.3f));
}
