#include "HB_PlayerState.h"

#include "AbilitySystemComponent.h"

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
}
