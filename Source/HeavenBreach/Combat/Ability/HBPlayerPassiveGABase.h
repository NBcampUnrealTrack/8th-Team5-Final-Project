// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "HBPlayerPassiveGABase.generated.h"

class AHBPlayerCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * 패시브 공통 GA
 * 효과 계산은 플레이어 캐릭터에 있는 깔때기(ModifyManaCost / ModifyCooldown / ModifyOutgoingDamage)가 그 태그를 읽어서 한다.
 */
UCLASS()
class HEAVENBREACH_API UHBPlayerPassiveGABase : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHBPlayerPassiveGABase();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 로드아웃이 바뀌면 ApplyLoadout 의 CancelAbilityHandle 로 여기 들어온다.
	// 붙인 GE 를 핸들을 통해 반드시 걷어야 한다. 안 걷으면 패시브를 뺐는데 최대체력이 남는다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 보유만으로 상시 걸리는 효과. 강인한 육체의 최대체력 GE.
	// 태그만 필요한 패시브는 비워둔다.
	UPROPERTY(EditDefaultsOnly, Category = "Passive")
	TSubclassOf<UGameplayEffect> SelfPassiveEffect;

private:
	UAbilitySystemComponent* GetASC(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 붙인 GE 핸들. EndAbility 에서 걷으려고 들고 있는다.
	// 멤버로 상태를 들고 있으므로 InstancedPerActor 가 필수다.
	TArray<FActiveGameplayEffectHandle> PassiveHandles;
};
