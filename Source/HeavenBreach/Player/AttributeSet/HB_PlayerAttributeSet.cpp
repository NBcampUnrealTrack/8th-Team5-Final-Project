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
	InitPlayer_LifeStealRatio(0.f);
	InitPlayer_BasicAttackManaCostMultiplier(1.f);
	InitPlayer_LowHealthAttackBonus(0.f);
	// 중립값 0 이다. 실제 임계값은 패시브 GE 가 Override 로 지정한다.
	// 여기에 0.3 을 박아두면 밸런스가 임계값을 바꿀 때 C++ 을 고쳐야 한다.
	// GE 에서 이 모디파이어를 빼먹으면 Span 이 1 이 되어 체력 0% 에서야 최대가 된다.
	// 곡선이 완만해지는 것으로 테스트에서 바로 드러난다.
	InitPlayer_LowHealthAttackThreshold(0.f);

	// 장비 옵션 칸. 더하는 칸은 0, 곱하는 칸은 1 이 "아무 효과 없음" 이다.
	InitPlayer_MoveSpeedFlat(0.f);
	InitPlayer_ManaRegenFlat(0.f);
	InitPlayer_ManaRegenMultiplier(1.f);
	InitPlayer_SkillDamageMultiplier(1.f);
	InitPlayer_HeadshotDamageBonus(0.f);
	InitPlayer_DamageTakenMultiplier(1.f);
	InitPlayer_MeleeDamageTakenMultiplier(1.f);
	InitPlayer_HealingReceivedMultiplier(1.f);
	InitPlayer_HealthRegenFlat(0.f);
	InitPlayer_HealthRegenMultiplier(1.f);
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
	DOREPLIFETIME(ThisClass,Player_LifeStealRatio);
	DOREPLIFETIME(ThisClass,Player_BasicAttackManaCostMultiplier);
	DOREPLIFETIME(ThisClass,Player_LowHealthAttackBonus);
	DOREPLIFETIME(ThisClass,Player_LowHealthAttackThreshold);
	DOREPLIFETIME(ThisClass,Player_MoveSpeedFlat);
	DOREPLIFETIME(ThisClass,Player_ManaRegenFlat);
	DOREPLIFETIME(ThisClass,Player_ManaRegenMultiplier);
	DOREPLIFETIME(ThisClass,Player_SkillDamageMultiplier);
	DOREPLIFETIME(ThisClass,Player_HeadshotDamageBonus);
	DOREPLIFETIME(ThisClass,Player_DamageTakenMultiplier);
	DOREPLIFETIME(ThisClass,Player_MeleeDamageTakenMultiplier);
	DOREPLIFETIME(ThisClass,Player_HealingReceivedMultiplier);
	DOREPLIFETIME(ThisClass,Player_HealthRegenFlat);
	DOREPLIFETIME(ThisClass,Player_HealthRegenMultiplier);
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
	else if (Attribute == GetPlayer_BasicAttackManaCostMultiplierAttribute())
	{
		// 하한 0 이라야 전투 호흡의 증강 Free(마나 0)가 성립한다.
		// 상한 2 는 소비 증가 디버프용.
		NewValue = FMath::Clamp(NewValue, 0.f, 2.f);
	}
	else if (Attribute == GetPlayer_LowHealthAttackBonusAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 2.f);
	}
	else if (Attribute == GetPlayer_LowHealthAttackThresholdAttribute())
	{
		// 상한 0.95 인 이유: 1.0 이면 산식의 (1 - 임계) 가 0 이 되어 0 으로 나눈다.
		NewValue = FMath::Clamp(NewValue, 0.f, 0.95f);
	}
	else if (Attribute == GetPlayer_LifeStealRatioAttribute())
	{
		// 흡혈 100% 를 넘기면 때릴수록 체력이 차서 죽지 않는다.
		// 버프·증강·장비가 겹쳐도 상한을 넘지 않게 여기서 막는다.
		// 쳐낼 조건: 100% 초과 흡혈이 기획에 들어오면 상한을 올린다.
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
	else if (Attribute == GetPlayer_ManaRegenMultiplierAttribute()
		|| Attribute == GetPlayer_SkillDamageMultiplierAttribute()
		|| Attribute == GetPlayer_HealingReceivedMultiplierAttribute()
		|| Attribute == GetPlayer_HealthRegenMultiplierAttribute())
	{
		// 0 아래는 "회복하면 줄어든다 / 때리면 회복시킨다" 가 되어 버린다.
		// 상한 3 은 옵션이 겹쳐 폭주하는 것만 막는 안전선이다. 밸런스 값이 아니다.
		NewValue = FMath::Clamp(NewValue, 0.f, 3.f);
	}
	else if (Attribute == GetPlayer_HeadshotDamageBonusAttribute())
	{
		// -1 이면 머리 배율이 0 이 된다. 그래도 최종 피해는 AEnemyCharacterBase 의 최소 피해 1 이 받친다.
		NewValue = FMath::Clamp(NewValue, -1.f, 2.f);
	}
	else if (Attribute == GetPlayer_DamageTakenMultiplierAttribute()
		|| Attribute == GetPlayer_MeleeDamageTakenMultiplierAttribute())
	{
		// 방어력은 100/(100+D) 곡선이라 끝없이 쌓아도 0 이 안 되지만 이 배율은 직선이다.
		// 하한이 없으면 -50% 옵션 두 개로 무적이 된다.
		// 쳐낼 조건: 받는 피해 옵션을 방어력 곡선 안으로 옮기면 이 칸째로 지운다.
		NewValue = FMath::Clamp(NewValue, 0.5f, 2.f);
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

	// 배율이든 더하는 속도든 최종 속도가 바뀌므로 둘 다 다시 계산한다.
	if (Attribute == GetPlayer_MoveSpeedMultiplierAttribute()
		|| Attribute == GetPlayer_MoveSpeedFlatAttribute())
	{
		if (AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor()))
		{
			Character->RefreshMovementSpeedFromAttributes();
		}
		return;
	}

	// ����ġ�� ���� ���� ������ �Ѵ�.
	// Ŭ�󿡼� ���� �� ������ �������� �ο��� ü�¹ٰ� �� �� Ƥ��.
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
		// ȸ������ Ȱ������ ġ�� �ʴ´�. ���⼭ �� ������ ȸ���� ������ ȸ�� ������ ������ ���� �����Ѵ�.
		if (Data.EvaluatedData.Magnitude < 0.f)
		{
			// ��Ƽ���� Cost GE �� ĳ������ ConsumeMana �� ��ġ�� �ʾ� LastManaActivityTime �� �� �и���.
			// GE �� ������ ������ ��� ��θ� ���� �� ������ �޴´�.
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

void UHB_PlayerAttributeSet::OnRep_LifeStealRatio(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_LifeStealRatio, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_BasicAttackManaCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_BasicAttackManaCostMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_LowHealthAttackBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_LowHealthAttackBonus, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_LowHealthAttackThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_LowHealthAttackThreshold, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_MoveSpeedFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_MoveSpeedFlat, OldValue);

	// 클라도 자기 속도를 다시 계산해야 한다. 이동은 클라가 먼저 움직이고 서버가 확인하는 구조라
	// 서버만 빨라지면 서버가 계속 위치를 되돌려 캐릭터가 뒤로 끌려간다.
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(ASC->GetAvatarActor());
	if (!Character)
	{
		return;
	}
	Character->RefreshMovementSpeedFromAttributes();
}

void UHB_PlayerAttributeSet::OnRep_ManaRegenFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_ManaRegenFlat, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_ManaRegenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_ManaRegenMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_SkillDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_SkillDamageMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_HeadshotDamageBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_HeadshotDamageBonus, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_DamageTakenMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_MeleeDamageTakenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_MeleeDamageTakenMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_HealingReceivedMultiplier, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_HealthRegenFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_HealthRegenFlat, OldValue);
}

void UHB_PlayerAttributeSet::OnRep_HealthRegenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Player_HealthRegenMultiplier, OldValue);
}

float UHB_PlayerAttributeSet::CalculateCurrentOnMaxChanged(float Current, float OldMax, float NewMax)
{
	// 죽은 상태에서 최대치가 바뀌어도 되살아나면 안 된다. 0 이면 건드리지 않는다.
	if (Current <= 0.f)
	{
		return Current;
	}

	// 이전 최대치가 0 이면 비율을 낼 수 없다. 넘치는 만큼만 자른다.
	if (OldMax <= 0.f)
	{
		return FMath::Min(Current, NewMax);
	}

	// 비율 유지: 80% 였으면 바뀐 뒤에도 80%.
	//
	// 왜 "늘어난 만큼 더하기" 가 아닌가: 장비는 벗었다 입었다 할 수 있다.
	// 더하기 방식이면 최대 체력 목걸이를 벗었다 다시 끼는 것만으로 체력이 찬다 (무한 회복).
	// 무기를 바꿔 들 때도 같은 일이 생긴다 (장비 GE 는 바뀔 때마다 빼고 다시 건다).
	//
	// 쳐낼 조건: 최대 체력을 올리는 순간 바로 채워 주는 기획이 들어오면,
	// 그 효과 쪽에서 회복을 따로 주고 여기는 그대로 둔다.
	const float Ratio = Current / OldMax;
	return FMath::Clamp(Ratio * NewMax, 0.f, NewMax);
}
