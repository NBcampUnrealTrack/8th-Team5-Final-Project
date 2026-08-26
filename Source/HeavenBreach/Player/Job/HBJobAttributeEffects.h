#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBJobAttributeEffects.generated.h"

/** Temporary class baselines. A Job Data asset may replace these through DefaultAttributesEffect. */
UCLASS()
class HEAVENBREACH_API UGE_DefaultAttributes_Warrior : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_DefaultAttributes_Warrior();
};

UCLASS()
class HEAVENBREACH_API UGE_DefaultAttributes_Archer : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_DefaultAttributes_Archer();
};

UCLASS()
class HEAVENBREACH_API UGE_DefaultAttributes_Mage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_DefaultAttributes_Mage();
};
