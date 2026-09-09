// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "HB_CharacterSelectionWidget.generated.h"

/**
 * 로비의 캐릭터(직업) 선택 화면.
 * Warrior/Mage/Archer 버튼으로 임시 선택 -> Decision 버튼으로 서버에 확정 전달.
 */

class UButton;
class AHBLobbyPlayerState;

UCLASS()
class HEAVENBREACH_API UHB_CharacterSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	/** Fired when the Back button is pressed, so the owning widget (e.g. the lobby HUD) can switch menus. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CharacterSelection")
	void OnBackRequested();

	/** Fired once the server confirms the selection, e.g. to show a "confirmed" label or return to the lobby menu. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CharacterSelection")
	void OnSelectionConfirmed(FName ConfirmedJobId);

private:
	void SelectPendingJob(FName JobId);
	void UpdateButtonStyles();
	void ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const;
	UButton* GetButtonForJob(FName JobId) const;

	UFUNCTION()
	void HandleSelectedJobChanged(FName NewSelectedJobId);

	/** Chosen locally but not yet confirmed with the server. NAME_None = nothing chosen. */
	FName PendingJobId = NAME_None;

	FButtonStyle WarriorButtonStyle;
	FButtonStyle MageButtonStyle;
	FButtonStyle ArcherButtonStyle;

	UPROPERTY()
	TObjectPtr<AHBLobbyPlayerState> LobbyPlayerState;
};
