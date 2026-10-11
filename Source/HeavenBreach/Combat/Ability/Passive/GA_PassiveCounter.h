// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_PassiveCounter.generated.h"

/**
 * 근접 패시브 "받아치기". 평타 도중에도 패링할 수 있다. 증강 없음.
 * State.Melee.Counter 태그만 부여하고, 패링 처리는 캐릭터 TryParry 가 태그를 읽어 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_PassiveCounter : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_PassiveCounter();
};
