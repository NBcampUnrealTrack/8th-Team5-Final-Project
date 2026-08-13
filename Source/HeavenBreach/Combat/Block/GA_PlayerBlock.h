#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerBlock.generated.h"

UCLASS()
class HEAVENBREACH_API UGA_PlayerBlock : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_PlayerBlock();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	
	UFUNCTION()
	void OnBlockMontageCompleted();
	UFUNCTION()
	void OnBlockMontageCancelled();
	
	UPROPERTY(EditDefaultsOnly,Category="Block")
	float BlockMoveSpeedMultiplier = 0.5f;
	
	UPROPERTY(EditDefaultsOnly,Category="Block")
	TObjectPtr<UAnimMontage> AM_Block;
	
private:
	float CachedMaxWalkSpeed = 0.f;
};
