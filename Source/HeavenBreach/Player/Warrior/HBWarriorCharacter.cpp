#include "Player/Warrior/HBWarriorCharacter.h"

AHBWarriorCharacter::AHBWarriorCharacter() = default;

void AHBWarriorCharacter::OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem)
{
	Super::OnAbilitySystemInitialized(AbilitySystem);

	if (!HasAuthority() || bWarriorLoadoutInitialized || !AbilitySystem)
	{
		return;
	}

	bWarriorLoadoutInitialized = true;
	InitializeWarriorLoadout();
}
