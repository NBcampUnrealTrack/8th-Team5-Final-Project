// Copyright HeavenBreach Team5. All Rights Reserved.
#include "Interaction/InteractionPromptWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
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
}

void UInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (HoldProgressBar == nullptr)
	{
		return;
	}

	// 홀드가 없는 대상(0초)이거나 누르고 있지 않으면 0 이 온다. 그때는 바를 숨긴다.
	const float Progress = InteractionSource.IsValid() ? InteractionSource->GetHoldProgress() : 0.0f;
	HoldProgressBar->SetPercent(Progress);
	HoldProgressBar->SetVisibility(Progress > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
