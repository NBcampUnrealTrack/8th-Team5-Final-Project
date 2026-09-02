#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

FActiveGameplayEffectHandle HBWarriorEffects::ApplyEffect(
	UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!ASC || !EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}
	return ASC->ApplyGameplayEffectToSelf(
		EffectClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
}

FActiveGameplayEffectHandle HBWarriorEffects::ApplyTaggedDuration(
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

FActiveGameplayEffectHandle HBWarriorEffects::ApplyBattleCryBuff(
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

bool HBWarriorEffects::TryApplyCooldown(
	UAbilitySystemComponent* ASC, const FGameplayTag CooldownTag, const float Duration)
{
	if (!ASC || !CooldownTag.IsValid() || IsCooldownActive(ASC, CooldownTag))
	{
		return false;
	}
	return ApplyTaggedDuration(ASC, Duration, CooldownTag).IsValid();
}

bool HBWarriorEffects::IsCooldownActive(
	const UAbilitySystemComponent* ASC, const FGameplayTag CooldownTag)
{
	return ASC && CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag);
}

float HBWarriorEffects::GetCooldownRemaining(
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
