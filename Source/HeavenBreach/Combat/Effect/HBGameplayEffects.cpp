#include "Combat/Effect/HBGameplayEffects.h"

#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Item/ItemStatBinding.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"

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

	void AddSetByCallerModifier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute,
		const EGameplayModOp::Type Operation, const FGameplayTag DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
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
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute(),
		EGameplayModOp::Multiplicitive, WarriorTag::TAG_Data_BattleCry_AttackMultiplier);
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(),
		EGameplayModOp::Additive, WarriorTag::TAG_Data_BattleCry_MoveSpeedBonus);
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute(),
		EGameplayModOp::Additive, WarriorTag::TAG_Data_BattleCry_DefenseBonus);
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

UGE_GuardStanceSlow::UGE_GuardStanceSlow()
{
	// 방어를 놓는 시점을 GE 가 모르므로 무한이고, GA 가 직접 뗀다.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_MoveSpeedMultiplierAttribute(),
		EGameplayModOp::Multiplicitive, 0.5f);
}

UGE_EquipmentStats::UGE_EquipmentStats()
{
	// 벗을 때까지 유지된다. 떼는 것은 RefreshEquipmentStats 가 핸들로 한다.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 스탯 표를 그대로 모디파이어로 옮긴다. 스탯이 늘어도 여기는 안 고친다.
	// 표에 칸을 더하면 여기 모디파이어가 생기고, RefreshEquipmentStats 가 같은 표를 돌며 값을 채운다.
	for (const FItemStatBinding& Binding : ItemStats::GetBindings())
	{
		if (Binding.FlatAttribute.IsValid())
		{
			AddSetByCallerModifier(*this, Binding.FlatAttribute, EGameplayModOp::Additive, Binding.FlatDataTag);
		}

		if (!Binding.PercentAttribute.IsValid())
		{
			continue;
		}

		// 진짜 숫자 칸(공격력 등)은 곱하고, 배율 칸(이동속도 배율 등)은 더한다. 이유는 EItemPercentMode 주석.
		EGameplayModOp::Type PercentOp = EGameplayModOp::Additive;
		if (Binding.PercentMode == EItemPercentMode::MultiplyValue)
		{
			PercentOp = EGameplayModOp::Multiplicitive;
		}

		AddSetByCallerModifier(*this, Binding.PercentAttribute, PercentOp, Binding.PercentDataTag);
	}
}

UGE_ItemEffectTag::UGE_ItemEffectTag()
{
	// 벗을 때까지 유지된다. 떼는 것은 RefreshItemEffects 가 핸들로 한다.
	// 모디파이어는 없다. 태그는 붙이는 쪽이 스펙에 넣는다.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UGE_ItemSelfDamage::UGE_ItemSelfDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_HPAttribute(),
		EGameplayModOp::Additive, ItemTag::TAG_Data_ItemEffect_Magnitude);
}

UGE_FallDamage::UGE_FallDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_HPAttribute(),
		EGameplayModOp::Additive, AbilityTag::TAG_SetByCaller_Damage);
}

UGE_LightLandingPassive::UGE_LightLandingPassive()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddModifier(*this, UHB_PlayerAttributeSet::GetPlayer_FallDamageReductionAttribute(),
		EGameplayModOp::Additive, 0.4f);
}

UGE_LightLandingAugment::UGE_LightLandingAugment()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddSetByCallerModifier(*this, UHB_PlayerAttributeSet::GetPlayer_FallDamageReductionAttribute(),
		EGameplayModOp::Additive, AbilityTag::TAG_SetByCaller_AugmentMagnitude);
}
