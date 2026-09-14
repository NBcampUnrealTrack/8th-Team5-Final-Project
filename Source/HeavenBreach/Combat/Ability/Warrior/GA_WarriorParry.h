#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_WarriorParry.generated.h"

class UGameplayEffect;

/**
 * 패링 패시브. 하는 일은 없고 설정만 들고 있다.
 *
 * 왜 C++ 자식인가: 태그만 드는 패시브면 BP 로 충분하지만, 패링은 수치가 셋이다.
 * 그 값들이 캐릭터(감소율)와 방어 GA(창 길이)에 흩어져 있어서, UI 가 패링 카드에
 * "받는 피해 90% 감소" 를 데이터로 만들 수 없었다. 값의 주인을 여기 하나로 모은다.
 *
 * 작동방식: 판정은 여기서 하지 않는다. 피격 시점에 일어나는 일이라 어빌리티가
 * 관여할 수 없다. AHBPlayerCharacter::ApplyMeleeHit 이 이 설정을 읽어 판정하고,
 * GA_PlayerBlock 이 이 설정으로 창을 연다.
 *
 * 쳐낼 조건: 패링이 전 직업 공용이 되면 Warrior 를 이름에서 뺀다.
 * 설정이 더 늘어나 종류가 갈리기 시작하면 그때 나눈다.
 */
UCLASS()
class HEAVENBREACH_API UGA_WarriorParry : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	// 패링 성공 시 받는 피해 감소율. 일반 방어(방패의 DamageReduction)보다 커야 의미가 있다.
	UFUNCTION(BlueprintPure, Category = "Parry")
	float GetDamageReduction() const { return DamageReduction; }

	// 방어 진입 후 이 시간 안에 맞아야 패링이다.
	UFUNCTION(BlueprintPure, Category = "Parry")
	float GetWindowDuration() const { return WindowDuration; }

	// 창을 여는 GE. State.Warrior.ParryWindow 를 SetByCaller.Duration 만큼 부여한다.
	UFUNCTION(BlueprintPure, Category = "Parry")
	TSubclassOf<UGameplayEffect> GetWindowEffect() const { return WindowEffect; }

	// 설명의 {ParryWindow} {ParryReduction%} 를 이 값들로 답한다.
	// 값의 주인이 여기 하나라는 이 클래스의 취지 그대로다.
	virtual bool GetNamedSkillValue(FName InName, float& OutValue) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction = 0.9f;

	// 데디 서버에서는 여기에 핑만큼이 깎인다. 로컬에서 딱 맞게 잡으면 실제로는 모자란다.
	// 쳐낼 조건: 핑 보정을 넣게 되면 이 값이 "기본값" 이 되고 보정치가 더해진다.
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.0"))
	float WindowDuration = 0.3f;

	// Has Duration + SetByCaller.Duration + Granted Tag(State.Warrior.ParryWindow).
	// 비워 두면 창이 안 열려서 패링이 성립하지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> WindowEffect;
};
