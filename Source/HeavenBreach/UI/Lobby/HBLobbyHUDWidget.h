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

#pragma region SettingsMenu
	UPROPERTY(EditDefaultsOnly, Category="Lobby HUD|Settings")
	TSubclassOf<UHBSettingsWidget> SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHBSettingsWidget> SettingsWidget;

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

public:
	UWidget* GetContentWidget(EHBLobbyHUDMenu Menu) const;

	/** Lets a nested content widget (e.g. the character selection screen) switch the HUD back to a given menu without Blueprint wiring. */
	UFUNCTION(BlueprintCallable, Category = "Lobby HUD")
	void SwitchToMenu(EHBLobbyHUDMenu NewMenu) { SelectMenu(NewMenu); }

private:
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
