// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "HBPlayerPassiveGABase.generated.h"

class UGameplayEffect;

/**
 * 패시브 공통 GA
 *
 * 패시브는 스스로 계산하지 않는다. 태그를 들고만 있고, 실제 계산은
 * 플레이어 캐릭터의 깔때기(ModifyManaCost / ModifyCooldown / ModifyOutgoingDamage)가
 * 그 태그를 읽어서 한다.
 *
 * GE 하나만 붙이면 되는 패시브는 C++ 자식을 만들지 않는다.
 * 이 클래스를 부모로 BP 를 만들고 SelfPassiveEffect 만 채우면 끝난다.
 * 전투 호흡이 그 형태다. 로직이 필요한 패시브만 C++ 자식으로 만든다.
 *
 * 증강 목록과 출처 태그, 아바타 접근은 부모(UHBPlayerSkillGABase)에 있다.
 */
UCLASS()
class HEAVENBREACH_API UHBPlayerPassiveGABase : public UHBPlayerSkillGABase
{
	GENERATED_BODY()
public:
	UHBPlayerPassiveGABase();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 로드아웃이 바뀌면 ApplyLoadout 이 CancelAbilityHandle 로 이 어빌리티를 끝낸다.
	// 그때 붙여 둔 GE 를 반드시 떼야 한다. 안 떼면 패시브를 뺐는데도 최대 체력이 남는다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 이 패시브를 가진 동안 상시로 걸리는 효과. 최대 체력 증가 같은 것.
	// 태그만 필요한 패시브는 비워 둔다. 비어 있으면 아무것도 붙이지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Passive")
	TSubclassOf<UGameplayEffect> SelfPassiveEffect;

private:
	// 붙여 둔 상시 GE 핸들. EndAbility 에서 떼려고 들고 있는다.
	// 인스턴스마다 다른 값이므로 InstancedPerActor 가 필수다.
	TArray<FActiveGameplayEffectHandle> PassiveHandles;
};
