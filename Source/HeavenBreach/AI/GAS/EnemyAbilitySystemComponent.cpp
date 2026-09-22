// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/Enum/EAlertState.h"
#include "AI/GAS/EnemyMontageGABase.h"

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

void UEnemyAbilitySystemComponent::InitStats(float InMaxHP, float InAttackPower, float InAttackSpeedRate, float InDefense)
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
	SetNumericAttributeBase(UEnemyAttributeSetBase::GetAttackSpeedRateAttribute(), InAttackSpeedRate);

	// 방어력은 다른 값에 의존하지 않아 순서가 자유롭다.
	SetNumericAttributeBase(UEnemyAttributeSetBase::GetDefenseAttribute(), InDefense);
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

bool UEnemyAbilitySystemComponent::TryActivateWeightedAbilityByTag(FGameplayTag AbilityTag, float DistanceToTarget)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	TArray<FGameplayAbilitySpec> AbilityList;
	float TotalWeighte = 0.f;

	GetUsableAbilities(AbilityTag, DistanceToTarget, AbilityList, TotalWeighte);

	// 발동할 후보들이 없다...
	if (AbilityList.IsEmpty())
	{
		return false;
	}

	//가중치 총합 값으로 랜덤을 돌린다.
	float Roll = FMath::FRandRange(0.f, TotalWeighte);

	FGameplayAbilitySpecHandle Picked;

	float CurrentWeight = 0;
	// 가중치 범위의 어빌리티를 찾아서 발동시킨다.
	for (FGameplayAbilitySpec& Spec : AbilityList)
	{
		float AbilityWeight = Cast<UEnemyMontageGABase>(Spec.Ability)->GetSelectionWeight();

		CurrentWeight += AbilityWeight;

		// 누적치가 랜덤값을 넘어가면 해당 어빌리티가 실행된다.
		if (Roll <= CurrentWeight)
		{
			Picked = Spec.Handle;
			break;
		}
	}

	// 해당 어빌리티 실행.
	// 제대로 실행됨 : true, 안됨 : false
	return TryActivateAbility(Picked);
}

bool UEnemyAbilitySystemComponent::IsAbilityActiveByTag(FGameplayTag AbilityTag)
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

void UEnemyAbilitySystemComponent::GetUsableAbilities(FGameplayTag AbilityTag, float DistanceToTarget, TArray<FGameplayAbilitySpec>& OutList, float& OutTotalWeight)
{
	OutList.Reset();
	OutTotalWeight = 0.f;

	if (!AbilityTag.IsValid())
	{
		return;
	}

	// 활성화된 어빌리티 목록을 뒤진다
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		// 발동이 가능한 올바른태그의 어빌리티인지 확인
		if (!Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			continue;
		}

		// 가중치 값을 찾기위해 EnemyMontageGA로 캐스팅
		const UEnemyMontageGABase* MontageGA = Cast<UEnemyMontageGABase>(Spec.Ability);
		if (!MontageGA)
		{
			continue;
		}

		// 쿨타임, 차단태그등 발동이 가능한 어빌리티인지 확인
		if (!MontageGA->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get()))
		{
			continue;
		}

		// 거리확인
		if (!MontageGA->IsRange(DistanceToTarget))
		{
			continue;
		}

		OutList.Add(Spec);
		OutTotalWeight += MontageGA->GetSelectionWeight();
	}
}

bool UEnemyAbilitySystemComponent::HasAbilityByTag(FGameplayTag AbilityTag)
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

bool UEnemyAbilitySystemComponent::HasUsableAbilityByTag(FGameplayTag AbilityTag, float DistanceToTarget)
{
	TArray<FGameplayAbilitySpec> List;
	float Total = 0.f;

	GetUsableAbilities(AbilityTag, DistanceToTarget, List, Total);

	// 비어있다 = 사용 가능한 어빌리티 없음.
	bool bIsEmpty = List.IsEmpty();

	return !bIsEmpty;
}
