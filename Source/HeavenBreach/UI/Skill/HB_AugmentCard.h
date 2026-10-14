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
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnClicked() override;

	/**
	 * 마우스를 올리거나 내릴 때 불린다 (패드 포커스도 같다).
	 * WBP 에서 "On Hovered" / "On Unhovered" 이벤트로 애니메이션을 더 얹을 수도 있다.
	 */
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

	/** 마우스를 올렸을 때 나는 소리. 기본값은 Sound/UI/Skill/SFX_Augment_Hover. 비우면 소리를 내지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TSoftObjectPtr<class USoundBase> HoverSound = TSoftObjectPtr<class USoundBase>(
		FSoftObjectPath(TEXT("/Game/Sound/UI/Skill/SFX_Augment_Hover.SFX_Augment_Hover")));

	/**
	 * 올리면 카드가 살짝 커지고 위로 뜬다. 코드가 보간하므로 WBP 애니메이션이 없어도 된다.
	 * 카드 루트의 Render Transform 을 직접 쓰므로, 같은 값을 WBP 애니메이션으로도 움직이면 서로 덮어쓴다.
	 * 그럴 때는 이 옵션을 끄고 WBP 쪽 애니메이션만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hover Animation")
	bool bAnimateHover = true;

	/** 올렸을 때의 크기 배율. 1.0 이면 커지지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Hover Animation", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float HoverScale = 1.03f;

	/**
	 * 올렸을 때 위로 뜨는 픽셀 수. 기본은 0 (뜨지 않음).
	 * 카드의 마우스 판정 영역은 움직이지 않고 보이는 모양만 움직인다. 많이 띄울수록 눈에 보이는 카드와
	 * 마우스가 반응하는 범위가 어긋난다 (카드 윗부분에 올려도 반응이 없고, 비어 보이는 아래쪽이 반응한다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hover Animation", meta = (ClampMin = "0.0"))
	float HoverLiftPixels = 0.0f;

	/** 커지고 줄어드는 속도. 클수록 빠르다. */
	UPROPERTY(EditDefaultsOnly, Category = "Hover Animation", meta = (ClampMin = "1.0"))
	float HoverAnimSpeed = 14.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UImage* IconImage;

	FHBUpgradeCandidate CachedCandidate;

private:
	/** 0 = 평소, 1 = 올린 상태. 매 프레임 목표(bHovered)를 향해 보간한다. */
	float HoverAlpha = 0.0f;
	bool bHovered = false;
};
