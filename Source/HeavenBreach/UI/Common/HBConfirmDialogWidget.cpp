// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Common/HBConfirmDialogWidget.h"
#include "UI/Audio/HBUIAudioLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UHBConfirmDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// ESC 를 받으려면 포커스를 가질 수 있어야 한다. 띄우는 쪽이 SetKeyboardFocus 를 부른다.
	SetIsFocusable(true);

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &UHBConfirmDialogWidget::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &UHBConfirmDialogWidget::HandleCancelClicked);
	}
}

void UHBConfirmDialogWidget::Setup(const FText& Title, const FText& Message, const FText& ConfirmLabel, const FText& CancelLabel)
{
	if (Text_Title && !Title.IsEmpty())
	{
		Text_Title->SetText(Title);
	}
	if (Text_Message && !Message.IsEmpty())
	{
		Text_Message->SetText(Message);
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

void UHBConfirmDialogWidget::Close()
{
	RemoveFromParent();
}

FReply UHBConfirmDialogWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// ESC 는 취소다. 확인(종료)은 버튼으로만 누르게 해서 실수로 끄지 않는다.
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UHBConfirmDialogWidget::HandleConfirmClicked()
{
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Confirm);
	OnConfirmed.Broadcast();
}

void UHBConfirmDialogWidget::HandleCancelClicked()
{
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Cancel);
	OnCancelled.Broadcast();
	Close();
}
