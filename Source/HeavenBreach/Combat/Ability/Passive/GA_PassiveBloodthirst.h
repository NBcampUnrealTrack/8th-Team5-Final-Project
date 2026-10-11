// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveBloodthirst.generated.h"

/**
 * 근접 패시브 "피의 갈증". 적을 처치하면 최대 체력의 5% 를 회복한다.
 * 강화 증강(Augment.Passive.Bloodthirst.Boost)을 고르면 회복 비율이 단계별로 7/9/12% 가 된다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveBloodthirst : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveBloodthirst();
};
