// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveLightLanding.generated.h"

/**
 * 시작 패시브 "가벼운 착지". 낙하 피해를 40% 덜 받는다.
 * 강화 증강(Augment.Passive.LightLanding.Reduction)을 고르면 단계별로 55/65/75% 까지 늘어난다.
 *
 * 왜 C++ 인가: GE 와 증강 정의를 코드에 두면 에디터 작업이 직업 DA 등록 하나로 끝나고,
 * 수치가 테스트(HeavenBreach.FallDamage.LightLandingAugment)로 고정된다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveLightLanding : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveLightLanding();
};
