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
class UTextBlock;
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

	/** "현재 직업: OOO" 표시. 이 화면이 열려있을 때만 보이므로 로비 HUD가 아니라 이 위젯에 배치. */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* CurrentJobText;

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

	/** Optional Blueprint hook fired after Back already switched the lobby HUD back to the Lobby menu in C++ (e.g. for a transition animation). */
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
	FText GetJobDisplayName(FName JobId) const;

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
