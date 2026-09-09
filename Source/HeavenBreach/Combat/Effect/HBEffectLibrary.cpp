#include "Combat/Effect/HBEffectLibrary.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

FActiveGameplayEffectHandle HBEffects::ApplyEffect(
	UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!ASC || !EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}
	return ASC->ApplyGameplayEffectToSelf(
		EffectClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
}

FActiveGameplayEffectHandle HBEffects::ApplyTaggedDuration(
	UAbilitySystemComponent* ASC, const float Duration, const FGameplayTag GrantedTag)
	{
	if (!ASC || Duration <= 0.f || !GrantedTag.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpec Spec(GetDefault<UGE_WarriorTaggedDuration>(), ASC->MakeEffectContext(), 1.f);
	Spec.SetDuration(Duration, true);
	Spec.DynamicGrantedTags.AddTag(GrantedTag);
	return ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

FActiveGameplayEffectHandle HBEffects::ApplyBattleCryBuff(
	UAbilitySystemComponent* ASC, const float Duration, const float AttackMultiplier,
	const float MoveSpeedBonus, const float DefenseBonus)
{
	if (!ASC || Duration <= 0.f)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpec Spec(GetDefault<UGE_WarriorBattleCryBuff>(), ASC->MakeEffectContext(), 1.f);
	Spec.SetDuration(Duration, true);
	Spec.SetSetByCallerMagnitude(WarriorTag::TAG_Data_BattleCry_AttackMultiplier, AttackMultiplier);
	Spec.SetSetByCallerMagnitude(WarriorTag::TAG_Data_BattleCry_MoveSpeedBonus, MoveSpeedBonus);
	Spec.SetSetByCallerMagnitude(WarriorTag::TAG_Data_BattleCry_DefenseBonus, DefenseBonus);
	return ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

bool HBEffects::TryApplyCooldown(
	UAbilitySystemComponent* ASC, const FGameplayTag CooldownTag, const float Duration)
{
	if (!ASC || !CooldownTag.IsValid() || IsCooldownActive(ASC, CooldownTag))
	{
		return false;
	}
	return ApplyTaggedDuration(ASC, Duration, CooldownTag).IsValid();
}

bool HBEffects::IsCooldownActive(
	const UAbilitySystemComponent* ASC, const FGameplayTag CooldownTag)
{
	return ASC && CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag);
}

float HBEffects::GetCooldownRemaining(
	const UAbilitySystemComponent* ASC, const FGameplayTag CooldownTag)
{
	if (!ASC || !CooldownTag.IsValid()) return 0.f;
	FGameplayTagContainer Tags(CooldownTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags);
	const TArray<float> Times = ASC->GetActiveEffectsTimeRemaining(Query);
	float Longest = 0.f;
	for (const float Time : Times) Longest = FMath::Max(Longest, Time);
	return Longest;
}
