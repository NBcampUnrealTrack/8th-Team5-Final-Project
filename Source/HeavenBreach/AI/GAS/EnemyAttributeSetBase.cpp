// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h" 
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "AI/Enum/EAlertState.h"
#include "AI/EnemyCharacterBase.h"

UEnemyAttributeSetBase::UEnemyAttributeSetBase()
{
	// 게임 플레이 도중에는 절대 값을 직접 접근해서 변경하면 안됨!
	// GameplayEffect로 바꿔줘야한다.
	// 때에 따라서는 SetHP를 써도되긴한데...되도록 쓰지말자
	InitMaxHP(100.f);
	InitHP(100.f);
	InitAttackPower(10.f);
}

void UEnemyAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnemyAttributeSetBase, HP);
	DOREPLIFETIME(UEnemyAttributeSetBase, MaxHP);
	DOREPLIFETIME(UEnemyAttributeSetBase, AttackPower);
}

bool UEnemyAttributeSetBase::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	// UAttributeSet에서 공백이긴한데 엔진 업데이트로 공백이아닌뭐가 추가될 경우가 있을수 있음
	Super::PreGameplayEffectExecute(Data);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PreGameplayEffectExecute"));

	return true;
}

void UEnemyAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 이 함수는 모든 어트리뷰트에 대해 불린다. 어느 것인지 반드시 비교해야 한다.
	if (Attribute == GetHPAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHP());
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}


	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PreAttributeChange"));
}

void UEnemyAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PostAttributeChange"));
}

void UEnemyAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 값을 확정시켜줫다. 
	// 이거 안해주면 음수로 가버림
	if (Data.EvaluatedData.Attribute == GetHPAttribute())
	{
		// Instant GE 는 BaseValue 를 직접 바꾸므로 여기서 한 번 더 자른다.
		SetHP(FMath::Clamp(GetHP(), 0.f, GetMaxHP()));

		// 사망판정
		if (GetHP() <= 0.f)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				// 이미 사망중인 적에게 다시 부여x
				if (!ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
				{
					// 사망 어빌리티가 태그부여함
					const bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag::TAG_Ability_Death));

					// 사망 어빌리티가 없거나 발동 실패
					// 태그만이라도 붙여서 StateTree 가 Dead 로 넘어가게 한다
					if (!bActivated)
					{
						// 원래는 TAG_Ability_Death가 할일
						ASC->AddLooseGameplayTag(AbilityTag::TAG_State_Dead);

						UE_LOG(LogEnemyAI, Warning, TEXT("[GAS] %s 사망 어빌리티 없음 — 태그만 부여"),*GetNameSafe(ASC->GetAvatarActor()));
					}

					// TODO: 사망시 델리게이트 이벤트를 보내서 다른 매니저들이 알게만들어야할까...?

					// 죽은상태가 되었으니 죽었을시 처리하라 알림
					if (AEnemyCharacterBase* Char = Cast<AEnemyCharacterBase>(ASC->GetAvatarActor()))
					{
						Char->BecomeDead();
					}

					UE_LOG(LogEnemyAI, Log, TEXT("[GAS] %s 사망"),*GetNameSafe(ASC->GetAvatarActor()));
				}
			}
		}
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PostGameplayEffectExecute"));
}

void UEnemyAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	// 더미임
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, HP, OldValue);
}

void UEnemyAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	// 더미임
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxHP, OldValue);
}
