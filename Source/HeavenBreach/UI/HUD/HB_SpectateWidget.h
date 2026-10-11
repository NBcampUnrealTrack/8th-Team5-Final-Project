#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_SpectateWidget.generated.h"

class APawn;
class UButton;
class UTextBlock;

/**
 * 관전 중 화면에 "누구를 보고 있는지"를 보여 주는 UI.
 *
 * 디자인은 WBP 로 만든다 (WBP_Spectate, 부모: 이 클래스). 컨트롤러의 Spectate Widget Class 에 지정한다.
 * WBP 를 지정하지 않으면 예전처럼 C++ 로 만든 글자 한 줄 UI 가 뜬다.
 *
 * 값을 채우기만 한다. 관전 대상을 바꾸는 입력(좌클릭)은 컨트롤러가 받는다.
 * 대상이 바뀌거나 살아 있는 팀원 수가 바뀔 때만 글자를 다시 쓴다.
 */
UCLASS()
class HEAVENBREACH_API UHB_SpectateWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 이전 팀원으로 넘기는 버튼. 관전할 수 있는 팀원이 둘 이상일 때만 눌린다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Prev;

	/** 다음 팀원으로 넘기는 버튼. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Next;

	/** 보고 있는 팀원의 이름. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TargetName;

	/** "2 / 3" (보고 있는 순번 / 관전 가능한 팀원 수). 팀원이 없으면 "-" 로 나온다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Counter;

	/**
	 * 보고 있는 대상이 바뀔 때 불린다. WBP 그래프에서 이름이 바뀔 때의 깜빡임 애니메이션 등에 쓴다.
	 * 처음 열릴 때도 한 번 불린다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spectate")
	void OnSpectateTargetChanged(const FText& TargetName);

private:
	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	/** WBP 없이 쓸 때만 만들어지는 글자 한 줄. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackLabel;

	TWeakObjectPtr<const APawn> LastTarget;
	int32 LastIndex = INDEX_NONE;
	int32 LastCount = INDEX_NONE;
};
