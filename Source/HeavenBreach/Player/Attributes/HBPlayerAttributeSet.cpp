#include "Player/Attributes/HBPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"

UHBPlayerAttributeSet::UHBPlayerAttributeSet()
{
	InitHealth(100.0f);
	InitMana(100.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
}

void UHBPlayerAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute()
		|| Attribute == GetManaAttribute()
		|| Attribute == GetAttackPowerAttribute()
		|| Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UHBPlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHBPlayerAttributeSet, Health, OldValue);
}

void UHBPlayerAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHBPlayerAttributeSet, Mana, OldValue);
}

void UHBPlayerAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHBPlayerAttributeSet, AttackPower, OldValue);
}

void UHBPlayerAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHBPlayerAttributeSet, Defense, OldValue);
}

void UHBPlayerAttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		UHBPlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(
		UHBPlayerAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(
		UHBPlayerAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(
		UHBPlayerAttributeSet, Defense, COND_None, REPNOTIFY_Always);
}
