// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveFocus.generated.h"

/**
 * 마법 시작 패시브 "집중". 마법 시전 시간이 25% 짧아진다(배율 0.75).
 * 강화 증강(Augment.Passive.Focus.Boost)을 고르면 배율이 단계별로 0.65/0.55/0.45 가 된다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveFocus : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveFocus();
};
