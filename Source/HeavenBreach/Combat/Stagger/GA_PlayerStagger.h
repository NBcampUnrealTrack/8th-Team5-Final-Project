#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerStagger.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 플레이어 경직. 적의 BP_GA_HitReact 와 같은 자리를 하는 플레이어판이다.
 *
 * 왜 넣었나: 패링당한 쪽을 멈춰 세워야 하는데, 적에게는 경직 GA 가 있지만
 * 플레이어에게는 없었다. 플레이어끼리 패링이 성립해도 아무 일이 없었다.
 *
 * 작동방식: AHBPlayerCharacter::ApplyStaggerTo 가 Ability.HitReact 로 발동시킨다.
 * 적과 같은 태그를 쓰므로 부르는 쪽은 상대가 적인지 플레이어인지 몰라도 된다.
 *
 * 경직 시간은 몽타주 길이다. 별도 GE 를 두지 않는다.
 * ActivationOwnedTags 가 어빌리티 수명과 정확히 같아서, 몽타주가 끝나면
 * State.Staggered 도 같이 사라진다. GE 로 하면 둘의 시간을 따로 맞춰야 한다.
 *
 * 스킬이 아니라 피격 반응이므로 UHBPlayerSkillGABase 를 상속하지 않는다.
 * 증강도 코스트도 쿨타임도 없다.
 *
 * 쳐낼 조건: 경직의 세기를 상황마다 다르게 해야 하면 몽타주를 여러 개 두고
 * 강타는 GA 를 나눈다. 지금은 한 종류뿐이다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerStagger : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerStagger();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 재생할 경직 몽타주. 비우면 발동해도 즉시 끝난다.
	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

	// 경직 몽타주는 공격속도를 따르지 않는다. 공속이 빠르다고 덜 맞아야 할 이유가 없다.
	// 경직 길이를 바꾸려면 이 값이나 몽타주 자체를 손댄다.
	UPROPERTY(EditDefaultsOnly, Category = "Stagger", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	// 경직 동안 걸어 둘 이동 감속 GE. 비우면 감속 없이 몽타주만 나간다.
	//
	// 왜 GE 인가: 이동속도는 어트리뷰트라 서버가 바꾸면 클라로 복제된다.
	// 공격(GA_PlayerAttackRE)·방어(GA_PlayerBlock)·액티브 스킬이 전부 같은 방식이다.
	//
	// GE 는 Infinite 여야 한다. 지속시간을 주면 경직보다 먼저 풀린다.
	// 경직 길이는 몽타주가 정하고, 이 GE 는 어빌리티 수명을 따른다.
	//
	// 쳐낼 조건: 경직 중 완전 정지가 필요해지면 GE 대신 이동 모드를 막는다.
	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TSubclassOf<UGameplayEffect> StaggerSlowEffect;

	// 완료든 중단이든 어빌리티를 끝낸다.
	// 중단은 다른 몽타주가 덮어썼다는 뜻이고, 그때는 경직을 붙들고 있을 이유가 없다.
	UFUNCTION()
	void OnMontageEnded();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// 감속 GE 를 붙인다. 떼는 것은 EndAbility 가 한다. 한 쌍으로 묶어 두었다.
	void ApplySlow(const FGameplayAbilityActorInfo* ActorInfo);

	// 걸어 둔 감속 GE. 종료 시 떼려고 들고 있는다.
	// 안 떼면 캐릭터가 영구히 느려진다.
	FActiveGameplayEffectHandle SlowHandle;
};
