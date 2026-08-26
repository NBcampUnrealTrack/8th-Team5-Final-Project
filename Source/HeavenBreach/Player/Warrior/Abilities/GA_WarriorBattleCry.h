#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WarriorBattleCry.generated.h"

UCLASS()
class HEAVENBREACH_API UGA_WarriorBattleCry : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WarriorBattleCry();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Battle Cry")
	float CooldownDuration = 10.f;

};
