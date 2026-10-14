// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * 범위 스킬(원소 구체 폭발·마력 폭발·서리 지대 도트·회전 베기)이 피해를 줄 대상 찾기.
 * 몬스터만이 아니라 팀원도 맞는다(화살·평타와 같은 팀킬 규칙). 시전자와 시체(State.Dead)는 뺀다.
 * 피해 거부(CanBeDamaged)는 부르는 쪽이 피해를 줄 때 본다(시전자를 알아야 해서).
 */
namespace HBSkillTargets
{
	/** Center 반경 Radius 안의 피해 받을 수 있는 액터(IDamageableInterface)를 한 번씩 OutTargets 에 더한다. 서버에서 부른다 */
	HEAVENBREACH_API void FindInSphere(const UWorld* World, const FVector& Center, float Radius, const AActor* Caster, TArray<AActor*>& OutTargets);
}
