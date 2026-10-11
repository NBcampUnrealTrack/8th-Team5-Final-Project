// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveResidualMana.generated.h"

/**
 * 마법 시작 패시브 "잔류 마력". 액티브 스킬 피해가 적에게 들어가면 장착 액티브 쿨다운을 줄인다(시전 1번에 1회). 계산은 캐릭터가 한다(State.Magic.ResidualMana).
 * 강화 증강(Augment.Passive.ResidualMana.Boost)을 고르면 쿨다운 감소 초가 단계별로 늘어난다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveResidualMana : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveResidualMana();
};
