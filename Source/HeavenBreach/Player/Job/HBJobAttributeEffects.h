#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBJobAttributeEffects.generated.h"

class UHBJobData;
struct FGameplayAttribute;

namespace HBJobAttributes
{
	/**
	 * 직업의 기본 능력치 하나를 읽는다. 로비 화면처럼 캐릭터가 아직 없을 때 "이 직업의 기본 체력은 얼마" 를 보여 주는 용도다.
	 *
	 * 어디서 읽나: 매치에서 캐릭터가 능력치를 받을 때 쓰는 것과 같은 GE (직업 DA 의 DefaultAttributesEffect,
	 * 없으면 아래 직업별 임시 GE) 의 Override 모디파이어 값이다. 장비나 증강이 붙은 값은 아니다.
	 *
	 * 주의: AHBPlayerCharacter 가 능력치를 줄 때 고르는 규칙과 같아야 한다. 그쪽 규칙을 바꾸면 여기도 같이 바꿀 것.
	 * 값을 못 찾으면 false (GE 가 없거나 그 능력치를 정하지 않거나 고정값이 아닐 때).
	 */
	HEAVENBREACH_API bool GetJobBaseAttribute(const UHBJobData* Job, const FGameplayAttribute& Attribute, float& OutValue);
}

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
