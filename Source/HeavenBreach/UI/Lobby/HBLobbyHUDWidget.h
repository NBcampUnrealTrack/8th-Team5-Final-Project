// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "HBLobbyHUDWidget.generated.h"

class UButton;
class UWidget;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EHBLobbyHUDMenu : uint8
{
	Lobby,
	ChoiceJob,
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

#pragma region LobbyActionButtons
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Lobby;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ChoiceJob;

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
	TObjectPtr<UWidget> Widget_ChoiceJob;

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
	/** Lets a nested content widget (e.g. the character selection screen) switch the HUD back to a given menu without Blueprint wiring. */
	UFUNCTION(BlueprintCallable, Category = "Lobby HUD")
	void SwitchToMenu(EHBLobbyHUDMenu NewMenu) { SelectMenu(NewMenu); }

private:
	UFUNCTION()
	void HandleLobbyClicked();

	UFUNCTION()
	void HandleChoiceJobClicked();

	UFUNCTION()
	void HandleStorageClicked();

	UFUNCTION()
	void HandleShopClicked();

	UFUNCTION()
	void HandleCraftClicked();

	void SelectMenu(EHBLobbyHUDMenu NewMenu);
	void UpdateButtonStyles();
	void ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const;
	UWidget* GetContentWidget(EHBLobbyHUDMenu Menu) const;

	FButtonStyle LobbyButtonStyle;
	FButtonStyle ChoiceJobButtonStyle;
	FButtonStyle StorageButtonStyle;
	FButtonStyle ShopButtonStyle;
	FButtonStyle CraftButtonStyle;
};
