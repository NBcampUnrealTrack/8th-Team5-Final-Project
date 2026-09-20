#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"
#include "GA_WarriorGuardStance.generated.h"

/**
 * 방어태세 패시브. 하는 일은 없고 태그만 든다.
 *
 * 이 태그가 있으면 GA_GuardStance 의 ActivationRequiredTags 가 통과되어,
 * 패링 창이 끝나도 R 을 쥐고 있는 동안 방어가 이어진다.
 *
 * 왜 C++ 자식인가: 부모 클래스 주석대로면 태그만 드는 패시브는 BP 로 충분하다.
 * 그런데 그렇게 하면 태그를 BP 의 ActivationOwnedTags 칸에 손으로 넣어야 하고,
 * 빠뜨리면 증강을 먹어도 방어태세가 조용히 안 켜진다. 실패가 눈에 안 보이는
 * 종류라 값의 주인을 코드에 못 박는다.
 *
 * 쳐낼 조건: 방어태세에 수치가 생기면(감소율, 자원 소모율 등) 그 값들을
 * 여기 두고 GA_GuardStance 가 읽게 한다.
 */
UCLASS()
class HEAVENBREACH_API UGA_WarriorGuardStance : public UHBPlayerPassiveGABase
{
	GENERATED_BODY()

public:
	UGA_WarriorGuardStance();
};
