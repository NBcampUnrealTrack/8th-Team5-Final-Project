// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveCombatInstinct.generated.h"

/**
 * 패시브 "전투 본능"(옛 이름 광전사의 분노). 체력이 낮을수록 공격력이 오른다.
 * 상시 GE(최대 +50%, 임계 30%)는 BP_GA_Passive_Warrior_BerserkerRage 가 정한다.
 * 이 클래스는 강화 증강만 정의한다. BP 의 부모를 이 클래스로 바꿔 쓴다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveCombatInstinct : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveCombatInstinct();
};
