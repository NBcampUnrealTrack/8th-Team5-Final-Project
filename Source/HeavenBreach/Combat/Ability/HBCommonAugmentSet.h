#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "HBCommonAugmentSet.generated.h"

/**
 * 스킬에 매이지 않는 증강 목록. 최대 체력·이동속도 같은 것.
 *
 * 왜 따로 두나: 스킬 증강은 GA 애셋의 Augments 배열에 사는데, 공통 강화는
 * 어느 스킬 것도 아니라 담을 자리가 없었다. 수집(CollectSkillAugments)과
 * 적용(ApplyAugment) 양쪽이 스킬 CDO 를 거치므로 뜨지도 적용되지도 않았다.
 *
 * 왜 DataTable 이 아닌가: 이 프로젝트에서 이미 시도하고 버린 방식이다.
 * UHB_SkillAugment 헤더 주석 참조 - 클라가 테이블을 직접 셔플해서
 * 클라마다 카드가 달랐고 고른 결과가 서버에 전달되지 않았다.
 * 그리고 Params 가 TMap 이라 테이블 행에 담기 불편하다.
 *
 * 쳐낼 조건: 공통 강화가 스킬 증강과 다른 필드를 요구하기 시작하면
 * FHBSkillAugment 재사용을 그만두고 전용 구조체로 나눈다.
 */
UCLASS()
class HEAVENBREACH_API UHBCommonAugmentSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment", meta = (TitleProperty = "AugmentTag"))
	TArray<FHBSkillAugment> Augments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TMap<FName, TSoftObjectPtr<UTexture2D>> AugmentIcons;

	// 태그로 하나를 찾는다. 없으면 nullptr.
	const FHBSkillAugment* FindAugment(FGameplayTag AugmentTag) const;
};
