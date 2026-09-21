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

	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* CardContainer; // 카드를 가로로 3장 담는 패널

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class UHB_AugmentCard> AugmentCardClass;

private:
	// 서버가 준 후보로 카드를 다시 만든다. 기존 카드는 전부 걷어낸다.
	void RebuildCards();

	AHB_PlayerState* GetHBPlayerState() const;

	// 화면이 열려 있는 동안 후보가 복제돼 오면 다시 그린다.
	// 열리는 시점에 아직 복제가 안 닿았을 수 있어서 이 경로가 필요하다.
	UFUNCTION()
	void HandleUpgradeCandidatesChanged();

	UFUNCTION()
	void OnAugmentCardClicked(FName ClickedCandidateId);
};
