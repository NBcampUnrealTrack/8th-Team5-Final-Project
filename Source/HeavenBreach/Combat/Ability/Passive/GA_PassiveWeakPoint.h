// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveWeakPoint.generated.h"

/**
 * 활 시작 패시브 "급소 포착". 헤드샷 피해가 30% 늘어난다.
 * 강화 증강(Augment.Passive.WeakPoint.Boost)을 고르면 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveWeakPoint : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveWeakPoint();
};
