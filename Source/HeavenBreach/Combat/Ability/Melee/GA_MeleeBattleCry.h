// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_MeleeBattleCry.generated.h"

/**
 * 전투의 함성(근접 액티브). 동작은 전부 부모(자버프)이고 BP_GA_Warrior_BattleCry 가 값을 정한다.
 * 이 클래스는 증강 목록(효율·지속·위력)만 정의한다. BP 의 부모를 이 클래스로 바꿔 쓴다.
 *
 * 왜 따로 만들었나: 증강은 C++ 생성자에 둔다(활·마법과 같은 규칙). BP 만 있던 스킬이라 둘 곳이 없었다.
 * Player/Warrior/Abilities 의 UGA_WarriorBattleCry 는 쓰이지 않는 옛 코드라 이름을 피했다.
 */
UCLASS()
class HEAVENBREACH_API UGA_MeleeBattleCry : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_MeleeBattleCry();
};
