// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "HB_AugmentCard.generated.h"

/**
 * 증강 선택 화면의 카드 한 장.
 *
 * 카드는 서버가 뽑아 복제한 FHBUpgradeCandidate 를 그대로 그린다.
 * 카드가 스스로 후보를 고르거나 데이터를 조회하지 않는다. 받은 것만 그린다.
 *
 * 클릭하면 CandidateId 만 위로 올린다. 실제 적용은 PlayerState 가 서버에서 한다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentCardClicked, FName, ClickedCandidateId);

UCLASS()
class HEAVENBREACH_API UHB_AugmentCard : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void InitCard(const FHBUpgradeCandidate& InCandidate);

	UPROPERTY(BlueprintAssignable, Category = "Augment Card|Events")
	FOnAugmentCardClicked OnAugmentCardClickedDelegate;

protected:
	virtual void NativeOnClicked() override;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UImage* IconImage;

	FHBUpgradeCandidate CachedCandidate;
};
