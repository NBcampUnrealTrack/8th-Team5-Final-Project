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
	Shop
};

UCLASS()
class HEAVENBREACH_API UHBLobbyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma region LobbyActionButtons
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Lobby;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_ChoiceJob;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Storage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Shop;
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
#pragma endregion

	UPROPERTY(BlueprintReadOnly, Category = "Lobby HUD")
	EHBLobbyHUDMenu SelectedMenu = EHBLobbyHUDMenu::Lobby;

	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby HUD")
	void OnLobbyMenuSelected(EHBLobbyHUDMenu NewMenu);

private:
	UFUNCTION()
	void HandleLobbyClicked();

	UFUNCTION()
	void HandleChoiceJobClicked();

	UFUNCTION()
	void HandleStorageClicked();

	UFUNCTION()
	void HandleShopClicked();

	void SelectMenu(EHBLobbyHUDMenu NewMenu);
	void UpdateButtonStyles();
	void ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const;
	UWidget* GetContentWidget(EHBLobbyHUDMenu Menu) const;

	FButtonStyle LobbyButtonStyle;
	FButtonStyle ChoiceJobButtonStyle;
	FButtonStyle StorageButtonStyle;
	FButtonStyle ShopButtonStyle;
};
