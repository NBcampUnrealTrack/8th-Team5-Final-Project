#include "HB_PlayerState.h"

#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

AHB_PlayerState::AHB_PlayerState()
{
#pragma region NetworkSettings
	bReplicates = true;
#pragma endregion 
	
#pragma region AbilitySystemComponent
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
#pragma endregion 
	
#pragma region AttributeSet
	AttributeSet = CreateDefaultSubobject<UHB_PlayerAttributeSet>(TEXT("AttributeSet"));
#pragma endregion
}

void AHB_PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_HPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnHealthChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnMaxHealthChanged);
	}
}

void AHB_PlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(Data.NewValue, AttributeSet->GetPlayer_MaxHP());
	}
}

void AHB_PlayerState::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(AttributeSet->GetPlayer_HP(), Data.NewValue);
	}
}