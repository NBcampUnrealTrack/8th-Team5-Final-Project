#include "Item/ItemInfoPopupWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UItemInfoPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UItemInfoPopupWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (!HasFocusedDescendants())
	{
		RemoveFromParent();
	}
}

FReply UItemInfoPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		RemoveFromParent();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UItemInfoPopupWidget::SetupInfo(const FString& InDisplayName, const FString& InDescription,
	bool bInIsRelic, const FString& InBenefitDescription, const FString& InRiskDescription,
	int32 InSellValue, UTexture2D* InIcon)
{
	if (NameText != nullptr)
	{
		NameText->SetText(FText::FromString(InDisplayName));
	}

	if (DescriptionText != nullptr)
	{
		DescriptionText->SetText(FText::FromString(InDescription));
		DescriptionText->SetVisibility(InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (SellValueText != nullptr)
	{
		const FText SellValueFormat = FText::FromString(TEXT("판매가: {0}"));
		SellValueText->SetText(FText::Format(SellValueFormat, FText::AsNumber(InSellValue)));
	}

	if (IconImage != nullptr)
	{
		if (InIcon != nullptr)
		{
			IconImage->SetBrushFromTexture(InIcon, false);
			IconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (BenefitText != nullptr)
	{
		if (bInIsRelic && !InBenefitDescription.IsEmpty())
		{
			BenefitText->SetText(FText::FromString(TEXT("버프: ") + InBenefitDescription));
			BenefitText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			BenefitText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RiskText != nullptr)
	{
		if (bInIsRelic && !InRiskDescription.IsEmpty())
		{
			RiskText->SetText(FText::FromString(TEXT("디버프: ") + InRiskDescription));
			RiskText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RiskText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}