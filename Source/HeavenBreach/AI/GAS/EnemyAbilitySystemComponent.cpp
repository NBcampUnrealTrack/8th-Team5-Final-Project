// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/Enum/EAlertState.h"

//void UEnemyAbilitySystemComponent::ApplyInitStats(TSubclassOf<UGameplayEffect> InitEffect, float InMaxHP)
//{
//	AActor* Owner = GetOwner();
//	if (!Owner || !Owner->HasAuthority() || !InitEffect)
//	{
//		return;   // GE 적용은 서버 전용
//	}
//
//
//	// Effect를 자신에게 적용하여 초기 스탯(MaxHP 등) 세팅
//	FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingSpec(InitEffect, 1.f, MakeEffectContext());
//
//	// 제대로 아닌것이 들어졌을 경우를 방어
//	if (!EffectSpecHandle.IsValid())
//	{
//		return;
//	}
//
//	EffectSpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("MaxHP")), InMaxHP);
//
//	ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
//	
//
//}

void UEnemyAbilitySystemComponent::InitStats(float InMaxHP, float InAttackPower)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;   // 어트리뷰트 변경은 서버 전용
	}

	// MaxHP가 먼저다. HP는 MaxHP를 상한으로 쓰므로 순서를 바꾸면 HP가 잘린다.
	SetNumericAttributeBase(UEnemyAttributeSetBase::GetMaxHPAttribute(), InMaxHP);
	SetNumericAttributeBase(UEnemyAttributeSetBase::GetHPAttribute(), InMaxHP);
	SetNumericAttributeBase(UEnemyAttributeSetBase::GetAttackPowerAttribute(), InAttackPower);
}

void UEnemyAbilitySystemComponent::SetupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& InAbilities)
{
	AActor* Owner = GetOwner();

	// 어빌리티 등록작업은 서버에서
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 배열로 받은 어빌리티들 등록
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : InAbilities)
	{
		// 배열에 항목만 추가하고 클래스를 안 고른 경우
		if (!AbilityClass)
		{
			continue;
		}

		// 어빌리티 등록
		GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Owner));
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[GAS] %s 어빌리티 %d개 부여"), *GetNameSafe(Owner), InAbilities.Num());
}

bool UEnemyAbilitySystemComponent::IsAbilityActiveByTag(FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	// 활성화된 어빌리티 목록을 뒤진다
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		// 해당 에셋태그를 사용하는 어빌리티가 존재하며 활성화중인지 확인
		if (Spec.Ability->GetAssetTags().HasTag(AbilityTag) && Spec.IsActive())
		{
			return true;
		}
	}

	return false;
}

bool UEnemyAbilitySystemComponent::HasAbilityByTag(FGameplayTag AbilityTag) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		// 어빌리티에 찾고자하는 assetTag가 있나 확인
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			return true;
		}
	}

	return false;
}
