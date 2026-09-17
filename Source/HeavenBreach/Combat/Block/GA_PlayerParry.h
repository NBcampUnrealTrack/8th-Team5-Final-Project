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

	// 창이 닫히는 시점. Task 4 가 여기에 후딜을, Task 5 가 인계를 얹는다.
	UFUNCTION()
	void OnWindowExpired();

	// 패링 창 길이. 기획 지정값 0.2초.
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.05"))
	float WindowDuration = 0.2f;

	// 창을 여는 GE 프로퍼티는 두지 않는다.
	// HBEffects::ApplyTaggedDuration 이 UGE_WarriorTaggedDuration 으로
	// 지속시간과 태그를 런타임에 넣는다. 쿨다운이 쓰는 것과 같은 경로다.
	// 애셋을 꽂는 칸이 없으니 "지정을 깜빡해 창이 안 열리는" 사고도 없다.
};
