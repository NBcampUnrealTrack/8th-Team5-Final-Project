// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveManaCycle.generated.h"

/**
 * 마법 시작 패시브 "마력 순환". 액티브 스킬 마나 소모가 20% 줄어든다(달리기·방어 제외). 계산은 액티브 GA 가 한다(State.Magic.ManaCycle).
 * 강화 증강(Augment.Passive.ManaCycle.Boost)을 고르면 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveManaCycle : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveManaCycle();
};
