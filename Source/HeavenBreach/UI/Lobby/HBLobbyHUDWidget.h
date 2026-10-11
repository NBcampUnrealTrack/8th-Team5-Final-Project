// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "HBLobbyHUDWidget.generated.h"

class UButton;
class UWidget;
class UWidgetSwitcher;
class UHBSettingsWidget;
class UHBConfirmDialogWidget;
class AHB_PlayerState;

UENUM(BlueprintType)
enum class EHBLobbyHUDMenu : uint8
{
	Lobby,
	ChoiceSkill,
	Storage,
	Shop,
	Craft
};

UCLASS()
class HEAVENBREACH_API UHBLobbyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 자식이 처리하지 않은 ESC 가 여기까지 올라온다. 열린 팝업이 먼저 ESC 를 가져가므로
	// 팝업이 없을 때만 게임 종료 안내창이 뜬다.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

#pragma region QuitDialog
	/** ESC 를 눌렀을 때 띄울 확인창. WBP_ConfirmDialog 를 지정한다. 비우면 ESC 가 아무 일도 하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit")
	TSubclassOf<UHBConfirmDialogWidget> ConfirmDialogClass;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit")
	FText QuitDialogTitle;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit", meta = (MultiLine = "true"))
	FText QuitDialogMessage;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit")
	FText QuitConfirmLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit")
	FText QuitCancelLabel;

	/** 확인창이 다른 창(설정창 등)보다 위에 오도록 Z 순서를 높게 둔다. 설정창이 100 이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby HUD|Quit")
	int32 ConfirmDialogZOrder = 200;

	UPROPERTY(Transient)
	TObjectPtr<UHBConfirmDialogWidget> QuitDialog;

	void OpenQuitDialog();
	void CloseQuitDialog();

	UFUNCTION()
	void HandleQuitConfirmed();

	UFUNCTION()
	void HandleQuitDialogCancelled();
#pragma endregion

#pragma region SettingsMenu
	UPROPERTY(EditDefaultsOnly, Category="Lobby HUD|Settings")
	TSubclassOf<UHBSettingsWidget> SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHBSettingsWidget> SettingsWidget;

	/**
	 * 설정 아이콘으로 쓸 WBP_MainLobby_IconButton. HUD 에 이 이름으로 배치하면 클릭 시 설정창이 열린다.
	 * 이전 배치(NaviBar 안의 IconButton_Settings)도 계속 찾는다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> SettingsIconButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	UFUNCTION()
	void OpenSettings();
#pragma endregion

#pragma region LobbyActionButtons
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Lobby;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ChoiceSkill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Storage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Shop;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Craft;
#pragma endregion

#pragma region LobbyContent
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> LobbyContentSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_ChoiceSkill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Storage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Shop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Craft;
#pragma endregion

	UPROPERTY(BlueprintReadOnly, Category = "Lobby HUD")
	EHBLobbyHUDMenu SelectedMenu = EHBLobbyHUDMenu::Lobby;

	/** Builds modular navigation before the initial menu selection is applied. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby HUD")
	void InitializeNavigation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby HUD")
	void OnLobbyMenuSelected(EHBLobbyHUDMenu NewMenu);

	/** 기존 개인 메뉴를 소유 PlayerState가 준비된 뒤 초기화한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby HUD")
	void InitializePlayerMenus();

public:
	UWidget* GetContentWidget(EHBLobbyHUDMenu Menu) const;

	/** 기존 컨트롤러 갱신 루프에서 호출하며 같은 PlayerState에는 한 번만 연결한다. */
	bool RefreshPlayerMenus();

	/** Lets a nested content widget (e.g. the character selection screen) switch the HUD back to a given menu without Blueprint wiring. */
	UFUNCTION(BlueprintCallable, Category = "Lobby HUD")
	void SwitchToMenu(EHBLobbyHUDMenu NewMenu) { SelectMenu(NewMenu); }

private:
	TWeakObjectPtr<AHB_PlayerState> InitializedPlayerState;

	UFUNCTION()
	void HandleLobbyClicked();

	UFUNCTION()
	void HandleChoiceSkillClicked();

	UFUNCTION()
	void HandleStorageClicked();

	UFUNCTION()
	void HandleShopClicked();

	UFUNCTION()
	void HandleCraftClicked();

	void SelectMenu(EHBLobbyHUDMenu NewMenu);
	void UpdateButtonStyles();
	void ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const;

	FButtonStyle LobbyButtonStyle;
	FButtonStyle ChoiceSkillButtonStyle;
	FButtonStyle StorageButtonStyle;
	FButtonStyle ShopButtonStyle;
	FButtonStyle CraftButtonStyle;
};
