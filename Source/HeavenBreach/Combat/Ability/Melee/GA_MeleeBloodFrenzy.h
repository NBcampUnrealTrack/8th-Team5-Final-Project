// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "GA_MeleeBloodFrenzy.generated.h"

/**
 * 피의 광란(근접 액티브). 동작은 전부 부모(자버프 = 흡혈)이고 BP_GA_Warrior_BloodFrenzy 가 값을 정한다.
 * 이 클래스는 증강 목록(효율·흡혈·지속)만 정의한다. BP 의 부모를 이 클래스로 바꿔 쓴다.
 * 출혈 증강은 적에게 지속 피해를 주는 경로가 없어 보류했다.
 */
UCLASS()
class HEAVENBREACH_API UGA_MeleeBloodFrenzy : public UHBPlayerActiveGABase
{
	GENERATED_BODY()

public:
	UGA_MeleeBloodFrenzy();
};
