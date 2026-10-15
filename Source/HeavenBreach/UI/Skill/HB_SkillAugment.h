// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_SkillAugment.generated.h"

/**
 * 레벨업 증강 선택 화면.
 *
 * 카드를 뽑지 않는다. 서버가 PlayerState 에서 후보를 뽑아 복제하고,
 * 이 화면은 CurrentUpgradeCandidates 를 그대로 그린다. 선택도 서버가 판정한다.
 *
 * 왜 클라가 뽑지 않나: 예전에는 이 위젯이 데이터테이블을 직접 셔플했다.
 * 그러면 클라마다 다른 카드가 뜨고, 고른 결과가 서버에 전달되지 않아
 * 증강이 실제로 적용되지 않았다.
 */

class AHBPlayerController;
class AHB_PlayerState;
class USoundBase;

UCLASS()
class HEAVENBREACH_API UHB_SkillAugment : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_SkillAugment();

	static UHB_SkillAugment* OpenOn(
		AHBPlayerController* OwningPC,
		TSubclassOf<UHB_SkillAugment> ScreenClass);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* CardContainer; // 카드를 가로로 3장 담는 패널

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class UHB_AugmentCard> AugmentCardClass;

	/**
	 * 카드 사이 간격(좌우 합). RebuildCards 가 매번 새로 만드는 런타임 카드에
	 * 직접 적용한다 - 디자이너에 배치해 둔 카드는 RebuildCards 에서 전부
	 * ClearChildren 으로 걷어내고 새로 만들기 때문에, 디자이너에서 Padding을
	 * 만져도 게임에는 반영되지 않는다. 간격은 반드시 여기서 조절할 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	float CardSpacing = 20.0f;

	/**
	 * 증강 화면이 열릴 때 나는 소리. 기본값은 Sound/UI/Skill/SFX_Augment_Open.
	 * WBP 의 Class Defaults 에서 바꿀 수 있고, 비우면 소리를 내지 않는다.
	 * 소프트 참조라 에셋이 없어도 로드 에러가 나지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> OpenSound;

	/** 카드를 골랐을 때 나는 소리. 기본값은 Sound/UI/Skill/SFX_Augment_Confirm. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> SelectSound;

private:
	/** 이 화면을 연 로컬 플레이어에게만 들린다. 소리가 비어 있으면 아무 일도 없다. */
	void PlayAugmentSound(const TSoftObjectPtr<USoundBase>& Sound) const;

	// 서버가 준 후보로 카드를 다시 만든다. 기존 카드는 전부 걷어낸다.
	void RebuildCards();

	AHB_PlayerState* GetHBPlayerState() const;

	// 카드를 한 번 고르면 켠다. 켜진 뒤로는 카드를 다시 그리지 않고 클릭도 받지 않는다.
	//
	// 왜 필요한가: 호스트에서는 선택 요청이 그 자리에서 적용되고 다음 후보까지 방송된다.
	// 화면은 닫히는 중이지만 스택의 페이드아웃 동안 남아 있어서, 다음 카드가 그려져
	// 미리 보였고 그 카드를 한 번 더 누르면 선택권이 하나 더 소모됐다.
	bool bSelectionSubmitted = false;

	// 고른 뒤에도 고를 증강이 남아 있어 화면을 닫지 않고 다음 후보를 기다리는 중이다.
	// 이 동안은 bSelectionSubmitted 로 클릭을 막아 두고, 다음 후보가 오면 둘 다 푼다.
	//
	// 왜 닫았다 다시 열지 않나: 클라이언트는 다음 후보가 복제돼 오기 전에 다시 열면
	// 방금 고른 카드가 또 그려지고, 그걸 누르면 서버가 거절해 선택이 증발한다.
	bool bAwaitingNextCandidates = false;

	// 화면이 열려 있는 동안 후보가 복제돼 오면 다시 그린다.
	// 열리는 시점에 아직 복제가 안 닿았을 수 있어서 이 경로가 필요하다.
	UFUNCTION()
	void HandleUpgradeCandidatesChanged();

	UFUNCTION()
	void OnAugmentCardClicked(FName ClickedCandidateId);
};
