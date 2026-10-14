// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveIronHeart.generated.h"

/**
 * 패시브 "강철 심장"(옛 이름 강인한 육체). 상시 GE(최대 체력 ×1.5)는 BP_GA_Passive_Warrior_StrongBody 가 정한다.
 * 이 클래스는 강화 증강만 정의한다. BP 의 부모를 이 클래스로 바꿔 쓴다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveIronHeart : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveIronHeart();
};
