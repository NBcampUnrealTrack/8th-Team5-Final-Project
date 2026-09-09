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
	InitPlayer_ManaCostFlat(0.f);
	InitPlayer_ManaCostMultiplier(1.f);
	InitPlayer_CooldownFlat(0.f);
	InitPlayer_CooldownMultiplier(1.f);
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
	DOREPLIFETIME(ThisClass,Player_ManaCostFlat);
	DOREPLIFETIME(ThisClass,Player_ManaCostMultiplier);
	DOREPLIFETIME(ThisClass,Player_CooldownFlat);
	DOREPLIFETIME(ThisClass,Player_CooldownMultiplier);
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
	else if (Attribute == GetPlayer_ManaCostMultiplierAttribute()
		|| Attribute == GetPlayer_CooldownMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.2f, 2.f);
	}
}

void UHB_PlayerAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (Attribute == GetPlayer_MoveSpeedMultiplierAttribute())
	{
		if (AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor()))
		{
			Character->RefreshMovementSpeedFromAttributes();
		}
		return;
	}

	// 현재치를 쓰는 것은 서버만 한다.
	// 클라에서 쓰면 곧 도착할 복제값과 싸워서 체력바가 한 번 튄다.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	if (Attribute == GetPlayer_MaxHPAttribute())
	{
		SetPlayer_HP(CalculateCurrentOnMaxChanged(GetPlayer_HP(), OldValue, NewValue));
		return;
	}

	if (Attribute == GetPlayer_MaxManaAttribute())
	{
		SetPlayer_Mana(CalculateCurrentOnMaxChanged(GetPlayer_Mana(), OldValue, NewValue));
		return;
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

	if (Data.EvaluatedData.Attribute == GetPlayer_ManaAttribute())
	{
		// 회복분은 활동으로 치지 않는다. 여기서 안 막으면 회복이 스스로 회복 지연을 갱신해 영구 정지한다.
		if (Data.EvaluatedData.Magnitude < 0.f)
		{
			// 액티브의 Cost GE 는 캐릭터의 ConsumeMana 를 거치지 않아 LastManaActivityTime 이 안 밀린다.
			// GE 로 마나가 빠지는 모든 경로를 여기 한 곳에서 받는다.
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				if (AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor()))
				{
					Character->NotifyManaActivity();
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

void UHB_PlayerAttributeSet::OnRep_ManaCostFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_ManaCostFlat, OldValue);
}
void UHB_PlayerAttributeSet::OnRep_ManaCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_ManaCostMultiplier, OldValue);
}
void UHB_PlayerAttributeSet::OnRep_CooldownFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_CooldownFlat, OldValue);
}
void UHB_PlayerAttributeSet::OnRep_CooldownMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_CooldownMultiplier, OldValue);
}

float UHB_PlayerAttributeSet::CalculateCurrentOnMaxChanged(float Current, float OldMax, float NewMax)
{
	// 죽은 상태에서 최대치 상승으로 HP가 올라가 되살아나 버린다. 0이면 건드리지 않는다.
	if (Current <= 0.f)
	{
		return Current;
	}

	// 상승: 오른 만큼 현재치도 같이 올린다.
	// 안 올리면 장비를 낄 때마다 체력바가 깎여 보인다.
	if (NewMax > OldMax)
	{
		return Current + (NewMax - OldMax);
	}

	// 하락: 현재치는 깎지 않는다. 다만 최대치를 넘어버린 만큼만 잘라낸다.
	if (Current > NewMax)
	{
		return NewMax;
	}

	return Current;
}
