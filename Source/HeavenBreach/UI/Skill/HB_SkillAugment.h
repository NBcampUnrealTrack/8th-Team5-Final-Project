// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "CommonActivatableWidget.h"
#include "HB_SkillAugment.generated.h"

/**
 * 
 */

class AHBPlayerController;

UCLASS()
class HEAVENBREACH_API UHB_SkillAugment : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_SkillAugment();

	void GenerateRandomAugments(FName EquippedSlotQ, FName EquippedSlotE, TArray<FName> EquippedPassives);

	static UHB_SkillAugment* OpenOn(
		AHBPlayerController* OwningPC,
		TSubclassOf<UHB_SkillAugment> ScreenClass);

protected:
	virtual void NativeOnActivated() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* CardContainer; // 카드를 가로로 3개 담을 패널

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class UHB_AugmentCard> AugmentCardClass;

	UPROPERTY(EditDefaultsOnly)
	class UDataTable* AugmentDataTable;

private:
	UFUNCTION()
	void OnAugmentCardClicked(FName ClickedAugmentId);
	
};
