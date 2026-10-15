// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Lobby/HBLobbyHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Settings/HBSettingsWidget.h"
#include "UI/Common/HBConfirmDialogWidget.h"
#include "UI/Skill/HB_SkillMainWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Inventory/StashComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Shop/ShopComponent.h"
#include "Craft/CraftComponent.h"

namespace
{
	void ClearTooltips(UUserWidget* Module)
	{
		if (!Module)
		{
			return;
		}

		auto Clear = [](UWidget* Widget)
		{
			if (Widget)
			{
				Widget->SetToolTip(nullptr);
				Widget->SetToolTipText(FText::GetEmpty());
			}
		};

		Clear(Module);
		if (Module->WidgetTree)
		{
			Module->WidgetTree->ForEachWidget(Clear);
		}
	}
}

void UHBLobbyHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeNavigation();

	// ESC 를 받으려면 이 위젯 안에 키보드 포커스가 있어야 한다.
	SetIsFocusable(true);
	if (UWorld* World = GetWorld())
	{
		// 뷰포트에 올라간 뒤에 포커스를 줘야 해서 한 틱 미룬다.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsInViewport())
			{
				SetKeyboardFocus();
			}
		}));
	}

	// 확인창 기본 문구. 클래스 디폴트에서 바꿀 수 있다.
	if (QuitDialogTitle.IsEmpty())
	{
		QuitDialogTitle = NSLOCTEXT("LobbyHUD", "QuitTitle", "게임 종료");
	}
	if (QuitDialogMessage.IsEmpty())
	{
		QuitDialogMessage = NSLOCTEXT("LobbyHUD", "QuitMessage", "게임을 종료하시겠습니까?");
	}
	if (QuitConfirmLabel.IsEmpty())
	{
		QuitConfirmLabel = NSLOCTEXT("LobbyHUD", "QuitConfirm", "종료");
	}
	if (QuitCancelLabel.IsEmpty())
	{
		QuitCancelLabel = NSLOCTEXT("LobbyHUD", "QuitCancel", "취소");
	}

	// 설정 아이콘 버튼은 HUD 에 직접 배치한 SettingsIconButton 을 먼저 쓴다.
	// 없으면 예전 배치(NaviBar 안의 IconButton_Settings)를 찾는다.
	UUserWidget* SettingsIcon = SettingsIconButton;
	if (!SettingsIcon)
	{
		if (UUserWidget* NavigationBar = Cast<UUserWidget>(GetWidgetFromName(TEXT("WBP_MainLobby_NaviBar"))))
		{
			SettingsIcon = Cast<UUserWidget>(NavigationBar->GetWidgetFromName(TEXT("IconButton_Settings")));
		}
	}

	if (SettingsIcon)
	{
		// 버튼 이름을 몰라도 되게 먼저 Button_Icon 을 찾고, 없으면 안의 첫 Button 을 쓴다.
		SettingsButton = Cast<UButton>(SettingsIcon->GetWidgetFromName(TEXT("Button_Icon")));
		if (!SettingsButton && SettingsIcon->WidgetTree)
		{
			SettingsIcon->WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!SettingsButton)
				{
					SettingsButton = Cast<UButton>(Widget);
				}
			});
		}

		// 설정 아이콘에는 툴팁이 필요 없다. 아이콘 위젯이 PreConstruct 에서 넣어 둔 툴팁을
		// 모듈 자신과 그 안의 모든 위젯에서 걷어낸다.
		ClearTooltips(SettingsIcon);

		if (SettingsButton)
		{
			// Opening the settings widget supplies its own sound.
			FButtonStyle Style = SettingsButton->GetStyle();
			Style.PressedSlateSound.SetResourceObject(nullptr);
			SettingsButton->SetStyle(Style);
			SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenSettings);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LobbyHUD | 설정 아이콘 안에서 Button 을 찾지 못했다. 설정창이 열리지 않는다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyHUD | 설정 아이콘 버튼을 찾지 못했다. WBP_LobbyHUD 에서 설정용 WBP_MainLobby_IconButton 의 이름을 SettingsIconButton 으로 지정할 것."));
	}

	if (Button_Lobby)
	{
		LobbyButtonStyle = Button_Lobby->GetStyle();
		Button_Lobby->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleLobbyClicked);
	}

	if (Button_ChoiceSkill)
	{
		ChoiceSkillButtonStyle = Button_ChoiceSkill->GetStyle();
		Button_ChoiceSkill->OnClicked.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleChoiceSkillClicked);
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
	RefreshPlayerMenus();
}

void UHBLobbyHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 매 프레임 확인할 필요는 없다.
	FocusCheckElapsed += InDeltaTime;
	if (FocusCheckElapsed < 0.1f)
	{
		return;
	}
	FocusCheckElapsed = 0.0f;

	if (!IsInViewport() || HasAnyUserFocus() || HasFocusedDescendants())
	{
		return;
	}

	// 설정창이나 종료 안내창이 떠 있으면 그쪽이 포커스를 가져가는 중이다. 뺏지 않는다.
	if ((SettingsWidget && SettingsWidget->IsInViewport()) || (QuitDialog && QuitDialog->IsInViewport()))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("LobbyHUD | 포커스가 없어 되찾는다 (ESC 를 받기 위해)"));
	SetKeyboardFocus();
}

bool UHBLobbyHUDWidget::RefreshPlayerMenus()
{
	const auto* Player = GetOwningPlayer();
	auto* State = GetOwningPlayerState<AHB_PlayerState>();
	if (!Player || !Player->IsLocalController() || !IsValid(State)
		|| State->GetOwner() != Player || !State->HasActorBegunPlay()) return false;
	if (InitializedPlayerState == State) return true;
	if (!IsValid(State->GetInventory()) || !IsValid(State->GetStash())
		|| !IsValid(State->GetEquipment()) || !IsValid(State->GetShop())
		|| (Widget_Craft && !IsValid(State->Craft))) return false;

	// 늦게 도착한 PlayerState에 기존 스킬 변경 알림을 연결한다.
	if (auto* Skills = Cast<UHB_SkillMainWidget>(Widget_ChoiceSkill))
		Skills->RefreshFromPlayerState();
	InitializePlayerMenus();
	InitializedPlayerState = State;
	return true;
}

FReply UHBLobbyHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// 설정창이 열려 있으면 ESC 는 설정창을 닫는 데 쓴다. 그 위에 종료 안내를 겹쳐 띄우지 않는다.
		if (SettingsWidget && SettingsWidget->IsInViewport())
		{
			SettingsWidget->CloseSettings();
			return FReply::Handled();
		}

		if (QuitDialog && QuitDialog->IsInViewport())
		{
			CloseQuitDialog();
			return FReply::Handled();
		}

		OpenQuitDialog();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHBLobbyHUDWidget::OpenQuitDialog()
{
	if (!ConfirmDialogClass || !GetOwningPlayer())
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyHUD | ConfirmDialogClass 가 지정되지 않아 종료 안내창을 열 수 없다."));
		return;
	}

	if (!QuitDialog)
	{
		QuitDialog = CreateWidget<UHBConfirmDialogWidget>(GetOwningPlayer(), ConfirmDialogClass);
		if (QuitDialog)
		{
			QuitDialog->OnConfirmed.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleQuitConfirmed);
			QuitDialog->OnCancelled.AddUniqueDynamic(this, &UHBLobbyHUDWidget::HandleQuitDialogCancelled);
		}
	}

	if (QuitDialog && !QuitDialog->IsInViewport())
	{
		QuitDialog->Setup(QuitDialogTitle, QuitDialogMessage, QuitConfirmLabel, QuitCancelLabel);
		QuitDialog->AddToViewport(ConfirmDialogZOrder);
		QuitDialog->SetKeyboardFocus();
	}
}

void UHBLobbyHUDWidget::CloseQuitDialog()
{
	if (QuitDialog && QuitDialog->IsInViewport())
	{
		QuitDialog->Close();
	}

	// 닫힌 뒤에도 ESC 를 계속 받도록 포커스를 HUD 로 되돌린다.
	HandleQuitDialogCancelled();
}

void UHBLobbyHUDWidget::HandleQuitDialogCancelled()
{
	SetKeyboardFocus();
}

void UHBLobbyHUDWidget::HandleQuitConfirmed()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UHBLobbyHUDWidget::NativeDestruct()
{
	InitializedPlayerState.Reset();
	if (QuitDialog)
	{
		QuitDialog->OnConfirmed.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleQuitConfirmed);
		QuitDialog->OnCancelled.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleQuitDialogCancelled);
		QuitDialog->Close();
		QuitDialog = nullptr;
	}

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

	if (Button_ChoiceSkill)
	{
		Button_ChoiceSkill->OnClicked.RemoveDynamic(this, &UHBLobbyHUDWidget::HandleChoiceSkillClicked);
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

void UHBLobbyHUDWidget::HandleChoiceSkillClicked()
{
	SelectMenu(EHBLobbyHUDMenu::ChoiceSkill);
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

	// WidgetSwitcher는 CommonUI의 활성화 이벤트를 호출하지 않는다.
	if (NewMenu == EHBLobbyHUDMenu::ChoiceSkill)
		if (auto* Skills = Cast<UHB_SkillMainWidget>(Widget_ChoiceSkill))
			Skills->RefreshFromPlayerState();
	OnLobbyMenuSelected(NewMenu);
}

void UHBLobbyHUDWidget::UpdateButtonStyles()
{
	if (Button_Lobby)
	{
		Button_Lobby->SetStyle(LobbyButtonStyle);
	}

	if (Button_ChoiceSkill)
	{
		Button_ChoiceSkill->SetStyle(ChoiceSkillButtonStyle);
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
	case EHBLobbyHUDMenu::ChoiceSkill:
		ApplySelectedStyle(Button_ChoiceSkill, ChoiceSkillButtonStyle);
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
	case EHBLobbyHUDMenu::ChoiceSkill:
		return Widget_ChoiceSkill;
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
