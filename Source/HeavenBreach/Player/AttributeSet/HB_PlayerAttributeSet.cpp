#include "HB_PlayerAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Net/UnrealNetwork.h"
#include "Player/PlayerState/HB_PlayerState.h"

UHB_PlayerAttributeSet::UHB_PlayerAttributeSet()
{
	InitPlayer_MaxHP(100.f);
	InitPlayer_HP(100.f);
}

void UHB_PlayerAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass,Player_HP);
	DOREPLIFETIME(ThisClass,Player_MaxHP);
}

void UHB_PlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetPlayer_HPAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.f,GetPlayer_MaxHP());
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
