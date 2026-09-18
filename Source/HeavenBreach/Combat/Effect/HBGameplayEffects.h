#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBGameplayEffects.generated.h"

/** Stable replicated definition used for cooldown/state tags with per-spec duration. */
UCLASS()
class HEAVENBREACH_API UGE_WarriorTaggedDuration : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorTaggedDuration();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorBattleCryBuff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorBattleCryBuff();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorCombatInstinctStack : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorCombatInstinctStack();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorKillingMomentumBuff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorKillingMomentumBuff();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorStrongBodyPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorStrongBodyPassive();
};

/**
 * 방어태세 유지 중 이동속도 감소.
 *
 * Infinite 다. 방어를 놓는 시점을 GE 가 알 수 없으므로 GA_GuardStance 가
 * 핸들을 들고 있다가 EndAbility 에서 직접 뗀다.
 *
 * 왜 C++ 인가: BP_GE_PlayerBlockSlow 가 있었지만 구 방어와 함께 지웠고,
 * 이 프로젝트는 수치만 있는 GE 를 여기 모아 두는 쪽이다. 애셋을 하나 더
 * 만들면 그걸 GA 에 꽂는 단계가 생기고, 빠뜨리면 감속 없이 방어가 된다.
 */
UCLASS()
class HEAVENBREACH_API UGE_GuardStanceSlow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_GuardStanceSlow();
};
