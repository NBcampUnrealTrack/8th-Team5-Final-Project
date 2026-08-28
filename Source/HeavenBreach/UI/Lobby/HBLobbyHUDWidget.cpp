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

	if (Button_MakeItem)
	{
		MakeItemButtonStyle = Button_MakeItem->GetStyle();
		Button_MakeItem->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleMakeItemClicked);
	}

	if (Button_Storage)
	{
		StorageButtonStyle = Button_Storage->GetStyle();
		Button_Storage->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleStorageClicked);
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

	if (Button_MakeItem)
	{
		Button_MakeItem->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleMakeItemClicked);
	}

	if (Button_Storage)
	{
		Button_Storage->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleStorageClicked);
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

void UHBLobbyHUDWidget::HandleMakeItemClicked()
{
	SelectMenu(EHBLobbyHUDMenu::MakeItem);
}

void UHBLobbyHUDWidget::HandleStorageClicked()
{
	SelectMenu(EHBLobbyHUDMenu::Storage);
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

	if (Button_MakeItem)
	{
		Button_MakeItem->SetStyle(MakeItemButtonStyle);
	}

	if (Button_Storage)
	{
		Button_Storage->SetStyle(StorageButtonStyle);
	}

	switch (SelectedMenu)
	{
	case EHBLobbyHUDMenu::Lobby:
		ApplySelectedStyle(Button_Lobby, LobbyButtonStyle);
		break;
	case EHBLobbyHUDMenu::ChoiceJob:
		ApplySelectedStyle(Button_ChoiceJob, ChoiceJobButtonStyle);
		break;
	case EHBLobbyHUDMenu::MakeItem:
		ApplySelectedStyle(Button_MakeItem, MakeItemButtonStyle);
		break;
	case EHBLobbyHUDMenu::Storage:
		ApplySelectedStyle(Button_Storage, StorageButtonStyle);
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
	case EHBLobbyHUDMenu::MakeItem:
		return Widget_MakeItem;
	case EHBLobbyHUDMenu::Storage:
		return Widget_Storage;
	default:
		return nullptr;
	}
}
