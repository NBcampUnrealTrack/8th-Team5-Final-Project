// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveSteadyStance.generated.h"

/**
 * 근접 패시브 "견고한 자세". 홀드 방어의 스태미나 소모가 30% 줄어든다(배율 0.70).
 * 강화 증강(Augment.Passive.SteadyStance.Boost)을 고르면 배율이 단계별로 0.60/0.50/0.40 이 된다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveSteadyStance : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveSteadyStance();
};
