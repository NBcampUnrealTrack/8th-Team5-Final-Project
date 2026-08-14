#include "Item/UItemSplitPopupWidget.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"

void UItemSplitPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (QuantitySlider != nullptr)
	{
		QuantitySlider->OnValueChanged.AddDynamic(this, &UItemSplitPopupWidget::HandleSliderChanged);
	}
	if (QuantityText != nullptr)
	{
		QuantityText->OnTextCommitted.AddDynamic(this, &UItemSplitPopupWidget::HandleTextCommitted);
	}
	if (ConfirmButton != nullptr)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UItemSplitPopupWidget::HandleConfirmClicked);
	}
	if (CancelButton != nullptr)
	{
		CancelButton->OnClicked.AddDynamic(this, &UItemSplitPopupWidget::HandleCancelClicked);
	}
}

void UItemSplitPopupWidget::SetupRange(int32 InMinCount, int32 InMaxCount)
{
	MinCount = FMath::Max(1, InMinCount);
	MaxCount = FMath::Max(MinCount, InMaxCount);
	CurrentValue = FMath::Clamp(MaxCount / 2, MinCount, MaxCount);

	if (QuantitySlider != nullptr)
	{
		QuantitySlider->SetMinValue(static_cast<float>(MinCount));
		QuantitySlider->SetMaxValue(static_cast<float>(MaxCount));
		QuantitySlider->SetValue(static_cast<float>(CurrentValue));
	}

	UpdateDisplay();
}

void UItemSplitPopupWidget::HandleSliderChanged(float Value)
{
	CurrentValue = FMath::Clamp(FMath::RoundToInt(Value), MinCount, MaxCount);
	UpdateDisplay();
}

void UItemSplitPopupWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const int32 Parsed = FCString::Atoi(*Text.ToString());
	CurrentValue = FMath::Clamp(Parsed, MinCount, MaxCount);

	if (QuantitySlider != nullptr)
	{
		QuantitySlider->SetValue(static_cast<float>(CurrentValue));
	}

	UpdateDisplay();
}

void UItemSplitPopupWidget::HandleConfirmClicked()
{
	OnSplitConfirmed.Broadcast(CurrentValue);
	OnPopupClosed.Broadcast();
	RemoveFromParent();
}

void UItemSplitPopupWidget::HandleCancelClicked()
{
	OnPopupClosed.Broadcast();
	RemoveFromParent();
}

void UItemSplitPopupWidget::UpdateDisplay()
{
	if (QuantityText != nullptr)
	{
		QuantityText->SetText(FText::AsNumber(CurrentValue));
	}
}

void UItemSplitPopupWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);

	if (!HasFocusedDescendants())
	{
		RemoveFromParent();
	}
}