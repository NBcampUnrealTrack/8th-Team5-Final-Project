// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Passive/GA_PassiveCounter.h"

#include "Combat/Effect/HBGameplayEffects.h"

UGA_PassiveCounter::UGA_PassiveCounter()
{
	SelfPassiveEffect = UGE_CounterPassive::StaticClass();
	// 증강 없음(설계). 태그만 부여한다.
}
