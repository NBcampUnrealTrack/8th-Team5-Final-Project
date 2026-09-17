// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Lobby/HBLobbyHUDWidget.h"

#include "Components/Button.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Settings/HBSettingsWidget.h"

void UHBLobbyHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeNavigation();

	if (UUserWidget* NavigationBar = Cast<UUserWidget>(GetWidgetFromName(TEXT("WBP_MainLobby_NaviBar"))))
	{
		if (UUserWidget* SettingsIcon = Cast<UUserWidget>(NavigationBar->GetWidgetFromName(TEXT("IconButton_Settings"))))
		{
			SettingsButton = Cast<UButton>(SettingsIcon->GetWidgetFromName(TEXT("Button_Icon")));
			if (SettingsButton)
			{
				SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenSettings);
			}
		}
	}

	if (Button_Lobby)
	{
		LobbyButtonStyle = Button_Lobby->GetStyle();
		Button_Lobby->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleLobbyClicked);
	}

	if (Button_ChoiceJob)
	{
		ChoiceJobButtonStyle = Button_ChoiceJob->GetStyle();
		Button_ChoiceJob->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleChoiceJobClicked);
	}

	if (Button_Storage)
	{
		StorageButtonStyle = Button_Storage->GetStyle();
		Button_Storage->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleStorageClicked);
	}

	if (Button_Shop)
	{
		ShopButtonStyle = Button_Shop->GetStyle();
		Button_Shop->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleShopClicked);
	}

	if (Button_Craft)
	{
		CraftButtonStyle = Button_Craft->GetStyle();
		Button_Craft->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleCraftClicked);
	}
	SelectMenu(EHBLobbyHUDMenu::Lobby);
}

void UHBLobbyHUDWidget::NativeDestruct()
{
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenSettings);
	}
	if (SettingsWidget)
	{
		SettingsWidget->CloseSettings();
		SettingsWidget = nullptr;
	}
	if (Button_Lobby)
	{
		Button_Lobby->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleLobbyClicked);
	}

	if (Button_ChoiceJob)
	{
		Button_ChoiceJob->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleChoiceJobClicked);
	}

	if (Button_Storage)
	{
		Button_Storage->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleStorageClicked);
	}

	if (Button_Shop)
	{
		Button_Shop->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleShopClicked);
	}

	if (Button_Craft)
	{
		Button_Craft->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleCraftClicked);
	}
	Super::NativeDestruct();
}

void UHBLobbyHUDWidget::OpenSettings()
{
	if (!SettingsWidgetClass || !GetOwningPlayer()) return;
	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UHBSettingsWidget>(GetOwningPlayer(), SettingsWidgetClass);
	}
	if (SettingsWidget && !SettingsWidget->IsInViewport())
	{
		SettingsWidget->AddToViewport(100);
		SettingsWidget->SetReturnFocusWidget(SettingsButton);
		SettingsWidget->SetKeyboardFocus();
	}
}

void UHBLobbyHUDWidget::HandleLobbyClicked()
{
	SelectMenu(EHBLobbyHUDMenu::Lobby);
}

void UHBLobbyHUDWidget::HandleChoiceJobClicked()
{
	SelectMenu(EHBLobbyHUDMenu::ChoiceJob);
}

void UHBLobbyHUDWidget::HandleStorageClicked()
{
	SelectMenu(EHBLobbyHUDMenu::Storage);
}

void UHBLobbyHUDWidget::HandleShopClicked()
{
	SelectMenu(EHBLobbyHUDMenu::Shop);
}

void UHBLobbyHUDWidget::HandleCraftClicked()
{
	SelectMenu(EHBLobbyHUDMenu::Craft);
}

void UHBLobbyHUDWidget::SelectMenu(EHBLobbyHUDMenu NewMenu)
{
	SelectedMenu = NewMenu;
	UpdateButtonStyles();

	if (LobbyContentSwitcher)
	{
		if (UWidget* ContentWidget = GetContentWidget(NewMenu))
		{
			LobbyContentSwitcher->SetActiveWidget(ContentWidget);
		}
	}

	OnLobbyMenuSelected(NewMenu);
}

void UHBLobbyHUDWidget::UpdateButtonStyles()
{
	if (Button_Lobby)
	{
		Button_Lobby->SetStyle(LobbyButtonStyle);
	}

	if (Button_ChoiceJob)
	{
		Button_ChoiceJob->SetStyle(ChoiceJobButtonStyle);
	}

	if (Button_Storage)
	{
		Button_Storage->SetStyle(StorageButtonStyle);
	}

	if (Button_Shop)
	{
		Button_Shop->SetStyle(ShopButtonStyle);
	}

	if (Button_Craft)
	{
		Button_Craft->SetStyle(CraftButtonStyle);
	}

	switch (SelectedMenu)
	{
	case EHBLobbyHUDMenu::Lobby:
		ApplySelectedStyle(Button_Lobby, LobbyButtonStyle);
		break;
	case EHBLobbyHUDMenu::ChoiceJob:
		ApplySelectedStyle(Button_ChoiceJob, ChoiceJobButtonStyle);
		break;
	case EHBLobbyHUDMenu::Storage:
		ApplySelectedStyle(Button_Storage, StorageButtonStyle);
		break;
	case EHBLobbyHUDMenu::Shop:
		ApplySelectedStyle(Button_Shop, ShopButtonStyle);
		break;
	case EHBLobbyHUDMenu::Craft:
		ApplySelectedStyle(Button_Craft, CraftButtonStyle);
		break;
	default:
		break;
	}
}

void UHBLobbyHUDWidget::ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const
{
	if (!Button)
	{
		return;
	}

	FButtonStyle SelectedStyle = DefaultStyle;
	SelectedStyle.SetNormal(DefaultStyle.Hovered);
	Button->SetStyle(SelectedStyle);
}

UWidget* UHBLobbyHUDWidget::GetContentWidget(EHBLobbyHUDMenu Menu) const
{
	switch (Menu)
	{
	case EHBLobbyHUDMenu::Lobby:
		return Widget_Lobby;
	case EHBLobbyHUDMenu::ChoiceJob:
		return Widget_ChoiceJob;
	case EHBLobbyHUDMenu::Storage:
		return Widget_Storage;
	case EHBLobbyHUDMenu::Shop:
		return Widget_Shop;
	case EHBLobbyHUDMenu::Craft:
		return Widget_Craft;
	default:
		return nullptr;
	}
}
