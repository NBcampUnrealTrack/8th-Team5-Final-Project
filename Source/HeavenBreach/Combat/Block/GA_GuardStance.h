#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GA_GuardStance.generated.h"

class UGameplayEffect;

/**
 * 방어태세. 패링 창이 끝난 뒤 R 을 쥐고 있으면 이어받는다.
 *
 * 패링과의 차이는 스태거뿐이다. 피해는 양쪽 모두 완전 무효다.
 * 상위호환인 것이 의도다 — 세션 중 증강으로 얻는 성장 보상이기 때문이다.
 * 대신 유지하는 동안 자원을 태우고 느려진다.
 *
 * 스스로 발동하지 않는다. GA_PlayerParry 만 이것을 켠다.
 * 방어태세를 안 배웠으면 ActivationRequiredTags 에서 발동이 막히고,
 * 패링 쪽은 그 실패를 그대로 후딜로 받는다.
 */
UCLASS()
class HEAVENBREACH_API UGA_GuardStance : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_GuardStance();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// R 을 뗐다는 신호. AHBPlayerCharacter::ReleaseParryInput 이 보낸다.
	UFUNCTION()
	void OnReleaseReceived(FGameplayEventData Payload);

private:
	// 유지 중 걸어 둔 감속 GE. 무한 지속이라 반드시 떼야 한다.
	FActiveGameplayEffectHandle SlowHandle;

	void RemoveSlow(const FGameplayAbilityActorInfo* ActorInfo);
};
