// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBConfirmDialogWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBConfirmDialogSignal);

/**
 * 확인/취소 두 버튼짜리 공용 안내창.
 * 무엇을 물을지는 Setup 으로 받고, 눌렸다는 사실만 델리게이트로 알린다.
 * 게임 종료, 방 나가기 등 여러 곳에서 문구만 바꿔 쓴다.
 *
 * 화면 전체를 덮는 어두운 배경까지 WBP 에서 만들어 둔다 (뒤쪽 버튼이 눌리지 않게).
 * 띄우는 쪽이 AddToViewport 로 올리고, 이 위젯이 Close 로 스스로 내린다.
 */
UCLASS(Abstract)
class HEAVENBREACH_API UHBConfirmDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 문구를 채운다. 비워 보내면 WBP 에 적어 둔 글자를 그대로 둔다. */
	void Setup(const FText& Title, const FText& Message, const FText& ConfirmLabel, const FText& CancelLabel);

	UFUNCTION(BlueprintCallable, Category = "Dialog")
	void Close();

	UPROPERTY(BlueprintAssignable, Category = "Dialog|Events")
	FHBConfirmDialogSignal OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Dialog|Events")
	FHBConfirmDialogSignal OnCancelled;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Message;

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
