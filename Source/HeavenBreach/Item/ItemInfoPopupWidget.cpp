#include "Item/ItemInfoPopupWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

namespace
{
	FText GetGradeDisplayText(EItemGrade InGrade)
	{
		switch (InGrade)
		{
		case EItemGrade::Rare:   return FText::FromString(TEXT("레어"));
		case EItemGrade::Epic:   return FText::FromString(TEXT("에픽"));
		case EItemGrade::Legend: return FText::FromString(TEXT("전설"));
		case EItemGrade::Normal:
		default:                 return FText::FromString(TEXT("일반"));
		}
	}

	FLinearColor GetGradeColor(EItemGrade InGrade)
	{
		switch (InGrade)
		{
		case EItemGrade::Rare:   return FLinearColor(0.357f, 0.616f, 1.0f);
		case EItemGrade::Epic:   return FLinearColor(0.753f, 0.482f, 0.941f);
		case EItemGrade::Legend: return FLinearColor(1.0f, 0.706f, 0.239f);
		case EItemGrade::Normal:
		default:                 return FLinearColor(0.941f, 0.914f, 0.839f);
		}
	}

	FLinearColor GetGradeBadgeColor(EItemGrade InGrade)
	{
		switch (InGrade)
		{
		case EItemGrade::Rare:   return FLinearColor(0.114f, 0.247f, 0.431f);
		case EItemGrade::Epic:   return FLinearColor(0.290f, 0.129f, 0.4f);
		case EItemGrade::Legend: return FLinearColor(0.420f, 0.227f, 0.047f);
		case EItemGrade::Normal:
		default:                 return FLinearColor(0.357f, 0.325f, 0.263f);
		}
	}
}

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

void UItemInfoPopupWidget::SetupInfo(const FString& InDisplayName, EItemGrade InGrade, const FString& InDescription,
	bool bInIsRelic, const FString& InBenefitDescription, const FString& InRiskDescription,
	int32 InSellValue, UTexture2D* InIcon)
{
	const FLinearColor GradeColor = GetGradeColor(InGrade);

	if (NameText != nullptr)
	{
		NameText->SetText(FText::FromString(InDisplayName));
		NameText->SetColorAndOpacity(FSlateColor(GradeColor));
	}
	if (GradeText != nullptr)
	{
		GradeText->SetText(GetGradeDisplayText(InGrade));
		GradeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (GradeBadgeBorder != nullptr)
	{
		GradeBadgeBorder->SetBrushColor(GetGradeBadgeColor(InGrade));
	}
	if (IconBorder != nullptr)
	{
		IconBorder->SetBrushColor(GradeColor);
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