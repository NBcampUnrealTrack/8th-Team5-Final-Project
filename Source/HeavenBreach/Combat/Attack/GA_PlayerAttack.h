#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/HitResult.h"
#include "GA_PlayerAttack.generated.h"

UCLASS()
class HEAVENBREACH_API UGA_PlayerAttack : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_PlayerAttack();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UPROPERTY(EditDefaultsOnly,Category="Attack")
	TObjectPtr<UAnimMontage> AM_Attack;
	
	UPROPERTY(EditDefaultsOnly,Category="Attack")
	float TracerRadius = 50.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Attack|Feel")
	TSubclassOf<UCameraShakeBase> ObjectHitCameraShake;
	
	UPROPERTY(EditDefaultsOnly,Category="Attack|Feel")
	TSubclassOf<UCameraShakeBase> CreatureHitCameraShake;
	
private:
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageCancelled();
	UFUNCTION()
	void OnHitCheckEvent(FGameplayEventData Payload);
	void PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo);
	void ResolveWallHit(const FHitResult& Hit);
	void ResolveCreatureHit(AActor* Target, const FHitResult& Hit, AActor* AttackerActor);
	
	TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;
};
