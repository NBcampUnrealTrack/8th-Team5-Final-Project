// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Lobby/HBLobbyHUDWidget.h"

#include "Components/Button.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"

void UHBLobbyHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	SelectMenu(EHBLobbyHUDMenu::Lobby);
}

void UHBLobbyHUDWidget::NativeDestruct()
{
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

	Super::NativeDestruct();
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
	default:
		return nullptr;
	}
}
