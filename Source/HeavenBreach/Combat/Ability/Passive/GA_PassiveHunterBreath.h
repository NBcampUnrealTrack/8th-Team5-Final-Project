// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveHunterBreath.generated.h"

/**
 * 활 시작 패시브 "사냥꾼의 호흡". 달리기 마나 소모가 30% 줄고 마나 회복이 20% 빨라진다.
 * 강화 증강(Augment.Passive.HunterBreath.Boost)을 고르면 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveHunterBreath : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveHunterBreath();
};
