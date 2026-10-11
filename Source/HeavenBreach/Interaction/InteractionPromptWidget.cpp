// Copyright HeavenBreach Team5. All Rights Reserved.
#include "Interaction/InteractionPromptWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Interaction/InteractionComponent.h"

void UInteractionPromptWidget::ShowPrompt(const FText& InPromptText)
{
	if (PromptText != nullptr)
	{
		PromptText->SetText(InPromptText);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInteractionPromptWidget::HidePrompt()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UInteractionPromptWidget::SetInteractionSource(UInteractionComponent* InSource)
{
	InteractionSource = InSource;
}

void UInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HoldProgressBar != nullptr)
	{
		HoldProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 링 머티리얼은 한 번만 인스턴스로 만들어 두고 매 프레임 값만 바꾼다.
	if (HoldRingImage != nullptr)
	{
		HoldRingMID = HoldRingImage->GetDynamicMaterial();
	}

	bHoldMode = false;
	KeyDim = 0.0f;
	if (HoldRingImage != nullptr)
	{
		HoldRingImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KeyDimOverlay != nullptr)
	{
		KeyDimOverlay->SetRenderOpacity(0.0f);
		KeyDimOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 홀드가 없는 대상(0초)이거나 누르고 있지 않으면 0 이 온다. 떼면 값이 0 으로 돌아가 링도 초기화된다.
	const float Progress = InteractionSource.IsValid() ? InteractionSource->GetHoldProgress() : 0.0f;
	const bool bHolding = Progress > 0.0f;

	if (HoldProgressBar != nullptr)
	{
		HoldProgressBar->SetPercent(Progress);
		HoldProgressBar->SetVisibility(bHolding ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (HoldRingMID != nullptr)
	{
		HoldRingMID->SetScalarParameterValue(HoldPercentParameter, Progress);
	}

	SetHoldMode(bHolding);

	// F 키 위의 어두운 막이 누르는 동안 서서히 진해지고 떼면 서서히 사라진다.
	if (KeyDimOverlay != nullptr)
	{
		const float Target = bHolding ? 1.0f : 0.0f;
		if (!FMath::IsNearlyEqual(KeyDim, Target, 0.01f))
		{
			KeyDim = FMath::FInterpTo(KeyDim, Target, InDeltaTime, DimFadeSpeed);
			KeyDimOverlay->SetRenderOpacity(KeyDim);
			KeyDimOverlay->SetVisibility(KeyDim > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UInteractionPromptWidget::SetHoldMode(bool bHolding)
{
	if (bHoldMode == bHolding)
	{
		return;
	}
	bHoldMode = bHolding;

	// 누르는 동안 글자만 숨긴다. Collapsed 가 아니라 Hidden 이라 자리를 유지하므로
	// 왼쪽 F 키가 글자 폭만큼 밀려 움직이지 않는다.
	if (PromptText != nullptr)
	{
		PromptText->SetVisibility(bHolding ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	// 링은 F 키 테두리 위에 겹쳐 있다가 누르는 동안만 보인다.
	if (HoldRingImage != nullptr)
	{
		HoldRingImage->SetVisibility(bHolding ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
