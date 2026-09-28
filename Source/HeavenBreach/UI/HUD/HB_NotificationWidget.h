// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_NotificationWidget.generated.h"

class UTextBlock;

/**
 * 화면 중앙에 잠깐 떴다 사라지는 범용 알림(Toast) 위젯.
 *
 * 레벨업 전용이 아니다 - 퀘스트 완료/아이템 획득 등 "잠깐 띄웠다 치우는" 알림이면
 * 뭐든 ShowNotification 하나로 큐에 넣는다. 이미 뭔가 보여주고 있으면 큐에 쌓였다가
 * 순서대로 뜬다 (동시에 겹쳐 뜨지 않는다).
 *
 * 페이드인/아웃 애니메이션은 WBP 쪽 책임이다 - OnNotificationShown/Hidden 이벤트가
 * 오면 Play Animation 노드로 처리한다. C++ 은 큐 순서와 타이밍만 관리한다.
 */
UCLASS()
class HEAVENBREACH_API UHB_NotificationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 알림을 큐에 넣는다. Header 는 비워도 된다(그럼 HeaderText 는 숨겨진다). */
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ShowNotification(const FText& InHeader, const FText& InBody);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BodyText;

	/** 페이드 애니메이션을 빼고, 알림이 화면에 순수하게 유지되는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Notification")
	float DisplayDuration = 2.5f;

	/** WBP 의 페이드아웃 애니메이션 길이와 맞춰줄 값(초). 이만큼 기다렸다가 다음 큐를 처리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Notification")
	float HideAnimDuration = 0.3f;

	/** 알림이 화면에 나타날 때 호출된다. WBP 에서 페이드인 애니메이션을 재생할 것. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Notification|Events")
	void OnNotificationShown();

	/** 알림이 사라지기 시작할 때 호출된다. WBP 에서 페이드아웃 애니메이션을 재생할 것. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Notification|Events")
	void OnNotificationHidden();

private:
	struct FHBNotificationRequest
	{
		FText Header;
		FText Body;
	};

	/** 큐에 남은 게 있으면 맨 앞 걸 꺼내서 보여준다. 없으면 그냥 끝낸다. */
	void ProcessQueue();

	void HandleDisplayTimeElapsed();
	void HandleHideAnimElapsed();

	TArray<FHBNotificationRequest> PendingQueue;

	bool bIsShowing = false;

	FTimerHandle DisplayTimerHandle;
	FTimerHandle HideTimerHandle;
};
