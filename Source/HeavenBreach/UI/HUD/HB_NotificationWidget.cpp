// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HUD/HB_NotificationWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UHB_NotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 평소엔 완전히 숨어 있어야 한다. Collapsed 라 레이아웃 자리도 안 차지한다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHB_NotificationWidget::ShowNotification(const FText& InHeader, const FText& InBody)
{
	PendingQueue.Add(FHBNotificationRequest{ InHeader, InBody });

	// 지금 아무것도 안 보여주고 있을 때만 바로 시작한다. 뭔가 보여주는 중이면
	// HandleHideAnimElapsed 에서 알아서 다음 큐를 꺼내 간다.
	if (!bIsShowing)
	{
		ProcessQueue();
	}
}

void UHB_NotificationWidget::ProcessQueue()
{
	if (PendingQueue.Num() == 0)
	{
		bIsShowing = false;
		return;
	}

	bIsShowing = true;

	const FHBNotificationRequest Request = PendingQueue[0];
	PendingQueue.RemoveAt(0);

	if (HeaderText)
	{
		HeaderText->SetText(Request.Header);
		HeaderText->SetVisibility(Request.Header.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (BodyText)
	{
		BodyText->SetText(Request.Body);
	}

	// SelfHitTestInvisible: 보이긴 해야 하지만 알림 뒤 게임 화면 클릭을 막으면 안 된다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	OnNotificationShown();

	GetWorld()->GetTimerManager().SetTimer(
		DisplayTimerHandle, this, &UHB_NotificationWidget::HandleDisplayTimeElapsed, DisplayDuration, false);
}

void UHB_NotificationWidget::HandleDisplayTimeElapsed()
{
	OnNotificationHidden();

	GetWorld()->GetTimerManager().SetTimer(
		HideTimerHandle, this, &UHB_NotificationWidget::HandleHideAnimElapsed, HideAnimDuration, false);
}

void UHB_NotificationWidget::HandleHideAnimElapsed()
{
	SetVisibility(ESlateVisibility::Collapsed);

	// 큐에 다음 게 있으면 바로 이어서 보여준다. bIsShowing 은 ProcessQueue 안에서 갱신된다.
	ProcessQueue();
}
