// Copyright HeavenBreach Team5. All Rights Reserved.
#include "Interaction/InteractionPromptWidget.h"
#include "Components/TextBlock.h"

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