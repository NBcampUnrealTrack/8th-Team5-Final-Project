// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_ConfirmPopup.generated.h"

class AHBPlayerController;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBConfirmPopupSignal);

/**
 * 확인/취소 두 버튼짜리 팝업. Modal 레이어에 올라가며 입력 모드와 Back(ESC) 처리를 스택이 맡는다.
 *
 * 로비 화면용 UHBConfirmDialogWidget 과 이름과 구성이 같다. 로비는 레이어 스택을 쓰지 않는 화면이라
 * 그쪽은 일반 위젯이고, 인게임(레이어 스택을 쓰는 화면)에서는 이 위젯을 쓴다.
 *
 * 무엇을 물을지는 Setup 으로 받고, 눌렀다는 사실만 델리게이트로 알린다.
 * 포커스는 취소 버튼에 둔다. 엔터를 눌러도 확인이 눌리지 않게 하려는 것이다.
 */
UCLASS()
class HEAVENBREACH_API UHB_ConfirmPopup : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_ConfirmPopup();

	/** Modal 레이어에 올린다. 문구는 활성화되기 전에 채워진다. 실패하면 nullptr. */
	static UHB_ConfirmPopup* OpenOn(
		AHBPlayerController* OwningPC,
		TSubclassOf<UHB_ConfirmPopup> PopupClass,
		const FText& Title,
		const FText& Message,
		const FText& Warning,
		const FText& ConfirmLabel,
		const FText& CancelLabel);

	/**
	 * 비워 보낸 문구는 WBP 에 적어 둔 글자를 그대로 둔다. 단 Warning 은 예외다.
	 * Warning 이 비어 있으면 경고 칸(Text_Warning)을 숨긴다. 경고가 필요 없을 때 빈 칸이 남지 않게 하려는 것이다.
	 */
	void Setup(const FText& Title, const FText& Message, const FText& Warning, const FText& ConfirmLabel, const FText& CancelLabel);

	UPROPERTY(BlueprintAssignable, Category = "Popup|Events")
	FHBConfirmPopupSignal OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Popup|Events")
	FHBConfirmPopupSignal OnCancelled;

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Message;

	/** 질문과 따로 두는 경고 문구 (예: 아이템을 잃는다는 안내). 경고가 없을 때는 숨겨진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Warning;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Confirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Confirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Cancel;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
