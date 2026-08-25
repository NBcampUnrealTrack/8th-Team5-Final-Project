// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_CharacterSelectionWidget.generated.h"

/**
 * 
 */

class UButton;

UCLASS()
class HEAVENBREACH_API UHB_CharacterSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UButton* WarriorSelectButton;

	UPROPERTY(meta = (BindWidget))
	UButton* MageSelectButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ArcherSelectButton;

	UPROPERTY(meta = (BindWidget))
	UButton* DecisionButton;

	UPROPERTY(meta = (BindWidget))
	UButton* BackButton;

	UFUNCTION()
	void OnWarriorClicked();

	UFUNCTION()
	void OnMageClicked();

	UFUNCTION()
	void OnArcherClicked();

	UFUNCTION()
	void OnDecisionClicked();

	UFUNCTION()
	void OnBackClicked();
	
};
