// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/Bow/GA_BowPiercingShot.h"
#include "GA_BowTripleShot.generated.h"

/**
 * 트리플 샷 (활 액티브, 기문 님 인계 3.2·3.3 "트리플"). 다중 연사 요청을 이 형태로 만들었다.
 * 관통 사격과 같은 흐름(Q/E 준비 → 좌클릭 유지로 당김 → DrawTime 이상 당긴 뒤 놓으면 발사, 우클릭 취소)이고,
 * 놓으면 부채꼴로 ArrowCount 발을 동시에 쏜다. 가운데 화살이 조준점, 나머지는 좌우 SpreadDegrees 간격. 관통 없음.
 * 화살은 발동당 1개 소모(관통 사격과 같다). 장전 화살 표시는 1개이고 관통 기류는 켜지 않는다.
 */
UCLASS()
class HEAVENBREACH_API UGA_BowTripleShot : public UGA_BowPiercingShot
{
	GENERATED_BODY()

public:
	UGA_BowTripleShot();

	virtual bool ShowsPiercingCharge() const override { return false; }

	// 테스트·UI 용. CDO 에서 불러도 된다.
	int32 GetArrowCount() const { return ArrowCount; }
	float GetSpreadDegrees() const { return SpreadDegrees; }

protected:
	virtual void FireSkillArrows(AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow> ArrowClassToUse, const FDataTableRowHandle& ArrowRow,
		float FinalCoef, const FHBArrowLaunchOptions& Options) override;

	/** 한 번에 쏘는 화살 수 */
	UPROPERTY(EditDefaultsOnly, Category = "TripleShot", meta = (ClampMin = "1"))
	int32 ArrowCount = 3;

	/** 이웃 화살 사이 좌우 각도 */
	UPROPERTY(EditDefaultsOnly, Category = "TripleShot", meta = (ClampMin = "0.0", Units = "deg"))
	float SpreadDegrees = 6.f;
};
