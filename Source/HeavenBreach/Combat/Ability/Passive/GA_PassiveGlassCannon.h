// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveGlassCannon.generated.h"

/**
 * 마법 시작 패시브 "유리 대포". 스킬 피해 +25%, 받는 피해 +15%.
 * 강화 증강(Augment.Passive.GlassCannon.Boost)을 고르면 스킬 피해 증가가 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveGlassCannon : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveGlassCannon();
};
