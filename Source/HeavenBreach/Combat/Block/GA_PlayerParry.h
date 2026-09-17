#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerParry.generated.h"

/**
 * 패링. R 을 누르는 즉시 발동해 짧은 판정 창을 연다.
 *
 * 왜 즉시 쏘나: 탭인지 홀드인지 판정하려고 기다리면 그 지연이 0.2초짜리
 * 창을 통째로 잡아먹는다. 먼저 쏘고, 창이 끝날 때 R 이 아직 눌려 있는지 본다.
 *
 * 판정은 여기서 하지 않는다. 피격 시점의 일이라 어빌리티가 관여할 수 없다.
 * AHBPlayerCharacter::ApplyMeleeHit 이 State.ParryWindow 를 보고 판정한다.
 *
 * 발동 조건은 방패 하나다. 직업은 보지 않는다 — 기획 17.3 이 이미 양손 무기와
 * 보조 장비의 동시 착용을 막으므로, "방패 = 패링" 만으로 직업이 갈린다.
 * 그 검사는 AHBPlayerCharacter::TryParry 가 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PlayerParry : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerParry();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 창이 닫히는 시점. 여기서 후딜을 걸지 말지 갈린다.
	UFUNCTION()
	void OnWindowExpired();

	// 패링이 성립하면 후딜을 면제한다. 성공에까지 벌을 주면 쓸 이유가 없다.
	// 신호는 AHBPlayerCharacter::ApplyMeleeHit 이 판정 직후에 보낸다.
	UFUNCTION()
	void OnParrySuccess(FGameplayEventData Payload);

	// 패링 창 길이. 기획 지정값 0.2초.
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.05"))
	float WindowDuration = 0.2f;

	// 헛쳤을 때 굳는 시간. 패링 연타를 막는 유일한 장치다.
	//
	// 공격은 막지 않는다. 후딜의 목적은 연타 방지이고, 그건 이 어빌리티가
	// State.ParryRecovery 로 스스로 막히는 것만으로 충분하다. 공격까지 묶는 것은
	// 별개의 밸런스 판단이라 플레이해 보고 정한다.
	//
	// 쳐낼 조건: 헛친 대가가 약하다고 판단되면 GA_PlayerAttack 의
	// ActivationBlockedTags 에 State.ParryRecovery 를 추가한다.
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.0"))
	float RecoveryDuration = 0.5f;

private:
	bool bParrySucceeded = false;

	// 창을 여는 GE 프로퍼티는 두지 않는다.
	// HBEffects::ApplyTaggedDuration 이 UGE_WarriorTaggedDuration 으로
	// 지속시간과 태그를 런타임에 넣는다. 쿨다운이 쓰는 것과 같은 경로다.
	// 애셋을 꽂는 칸이 없으니 "지정을 깜빡해 창이 안 열리는" 사고도 없다.
};
