// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Common/HB_ConfirmPopup.h"
#include "UI/Audio/HBUIAudioLibrary.h"
#include "UI/HB_UILayout.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "Player/HBPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

UHB_ConfirmPopup::UHB_ConfirmPopup()
{
	InputMode = EHBWidgetInputMode::Menu;

	// ESC(Back) 가 팝업을 닫는다.
	bIsBackHandler = true;
}

UHB_ConfirmPopup* UHB_ConfirmPopup::OpenOn(
	AHBPlayerController* OwningPC,
	TSubclassOf<UHB_ConfirmPopup> PopupClass,
	const FText& Title,
	const FText& Message,
	const FText& Warning,
	const FText& ConfirmLabel,
	const FText& CancelLabel)
{
	if (OwningPC == nullptr || PopupClass == nullptr || OwningPC->GetUILayout() == nullptr)
	{
		return nullptr;
	}

	return OwningPC->GetUILayout()->PushWidgetToLayer<UHB_ConfirmPopup>(
		UITag::TAG_UI_Layer_Modal,
		PopupClass,
		[&](UHB_ConfirmPopup& Popup)
		{
			Popup.Setup(Title, Message, Warning, ConfirmLabel, CancelLabel);
		});
}

void UHB_ConfirmPopup::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &UHB_ConfirmPopup::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &UHB_ConfirmPopup::HandleCancelClicked);
	}
}

void UHB_ConfirmPopup::Setup(const FText& Title, const FText& Message, const FText& Warning, const FText& ConfirmLabel, const FText& CancelLabel)
{
	if (Text_Title && !Title.IsEmpty())
	{
		Text_Title->SetText(Title);
	}
	if (Text_Message && !Message.IsEmpty())
	{
		Text_Message->SetText(Message);
	}
	if (Text_Warning)
	{
		// 경고가 없으면 칸을 숨긴다. WBP 에 적어 둔 글자가 남아 보이지 않게 한다.
		Text_Warning->SetText(Warning);
		Text_Warning->SetVisibility(Warning.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Text_Confirm && !ConfirmLabel.IsEmpty())
	{
		Text_Confirm->SetText(ConfirmLabel);
	}
	if (Text_Cancel && !CancelLabel.IsEmpty())
	{
		Text_Cancel->SetText(CancelLabel);
	}
}

UWidget* UHB_ConfirmPopup::NativeGetDesiredFocusTarget() const
{
	// 취소 버튼에 포커스를 둔다. 엔터나 패드의 확인 입력으로 실수로 나가지 않게 하려는 것이다.
	if (Button_Cancel)
	{
		return Button_Cancel;
	}

	return Super::NativeGetDesiredFocusTarget();
}

FReply UHB_ConfirmPopup::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHB_ConfirmPopup::HandleConfirmClicked()
{
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Confirm);
	OnConfirmed.Broadcast();
	DeactivateWidget();
}

void UHB_ConfirmPopup::HandleCancelClicked()
{
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Cancel);
	OnCancelled.Broadcast();
	DeactivateWidget();
}
