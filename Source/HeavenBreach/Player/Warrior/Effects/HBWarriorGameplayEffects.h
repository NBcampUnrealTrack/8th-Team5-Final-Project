#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBWarriorGameplayEffects.generated.h"

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
