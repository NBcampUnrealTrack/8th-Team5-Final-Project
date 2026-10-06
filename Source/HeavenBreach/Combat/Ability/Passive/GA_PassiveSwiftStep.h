// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveSwiftStep.generated.h"

/**
 * 활 시작 패시브 "민첩한 발걸음". 이동속도가 10% 빨라진다.
 * 강화 증강(Augment.Passive.SwiftStep.Boost)을 고르면 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveSwiftStep : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveSwiftStep();
};
