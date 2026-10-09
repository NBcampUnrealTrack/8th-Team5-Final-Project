// Copyright HeavenBreach Team5. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UWidget;
class UMaterialInstanceDynamic;
class UInteractionComponent;

/**
 *  플레이어가 상호작용 가능한 대상을 바라볼 때 화면에 표시되는 프롬프트 위젯.
 *  UInteractionComponent::OnFocusChanged / GetCurrentPrompt()의 결과를 그대로 받아 표시한다.
 */
UCLASS()
class HEAVENBREACH_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ShowPrompt(const FText& InPromptText);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void HidePrompt();

	// 홀드 진행률을 읽어 올 곳. 프롬프트를 만든 캐릭터가 넣어 준다.
	void SetInteractionSource(UInteractionComponent* InSource);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;

	// F 를 누르고 있는 동안 채워지는 바. WBP 에 없으면 표시만 안 된다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HoldProgressBar;

	// Optional. Ring drawn around the F key. Its material has a 0..1 scalar parameter
	// (HoldPercentParameter) that fills the border clockwise. Shown only while holding.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HoldRingImage;

	// Optional. Dark overlay placed over the F key. Fades in while holding, fades out on release.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> KeyDimOverlay;

	// How fast the overlay fades (per second).
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "1.0"))
	float DimFadeSpeed = 14.0f;

	// Name of the 0..1 scalar parameter in the ring material.
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	FName HoldPercentParameter = TEXT("Percent");

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<UInteractionComponent> InteractionSource;

	void SetHoldMode(bool bHolding);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HoldRingMID;

	bool bHoldMode = false;

	float KeyDim = 0.0f;
};