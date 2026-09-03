#include "HB_PlayerAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Net/UnrealNetwork.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/HBPlayerCharacter.h"

UHB_PlayerAttributeSet::UHB_PlayerAttributeSet()
{
	InitPlayer_MaxHP(100.f);
	InitPlayer_HP(100.f);
	InitPlayer_Mana(100.f);
	InitPlayer_MaxMana(100.f);
	InitPlayer_AttackPower(10.f);
	InitPlayer_AttackSpeed(1.f);
	InitPlayer_Defense(0.f);
	InitPlayer_MoveSpeedMultiplier(1.f);
}

void UHB_PlayerAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass,Player_HP);
	DOREPLIFETIME(ThisClass,Player_MaxHP);
	DOREPLIFETIME(ThisClass,Player_Mana);
	DOREPLIFETIME(ThisClass,Player_MaxMana);
	DOREPLIFETIME(ThisClass,Player_AttackPower);
	DOREPLIFETIME(ThisClass,Player_AttackSpeed);
	DOREPLIFETIME(ThisClass,Player_Defense);
	DOREPLIFETIME(ThisClass,Player_MoveSpeedMultiplier);
}

void UHB_PlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetPlayer_HPAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.f,GetPlayer_MaxHP());
	}
	else if (Attribute == GetPlayer_AttackSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	else if (Attribute == GetPlayer_ManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetPlayer_MaxMana());
	}
	else if (Attribute == GetPlayer_MoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	else if (Attribute == GetPlayer_MaxHPAttribute()
		|| Attribute == GetPlayer_MaxManaAttribute()
		|| Attribute == GetPlayer_AttackPowerAttribute()
		|| Attribute == GetPlayer_DefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UHB_PlayerAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetPlayer_MoveSpeedMultiplierAttribute())
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor()))
			{
				Character->RefreshMovementSpeedFromAttributes();
			}
		}
	}
}

void UHB_PlayerAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	if (Data.EvaluatedData.Attribute == GetPlayer_HPAttribute())
	{
		SetPlayer_HP(FMath::Clamp(GetPlayer_HP(),0.f,GetPlayer_MaxHP()));
		
		if (GetPlayer_HP() <= 0.f)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				if (!ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
				{
					ASC->AddLooseGameplayTag(AbilityTag::TAG_State_Dead);
				}
				
				if (AHB_PlayerState* HBPlayerState = Cast<AHB_PlayerState>(GetOwningActor()))
				{
					if (AHeavenBreachGameMode* GameMode = HBPlayerState->GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>())
					{
						GameMode->NotifyPlayerDied(HBPlayerState);
					}
				}
			}
		}
	}
}

void UHB_PlayerAttributeSet::OnRep_HP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_HP,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_MaxHP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_MaxHP,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_Mana,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_AttackPower,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_AttackSpeed,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass,Player_Defense,OldValue);
}

void UHB_PlayerAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_MaxMana, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_MoveSpeedMultiplier, OldValue);
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor()))
		{
			Character->RefreshMovementSpeedFromAttributes();
		}
	}
}

