#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UItemSplitPopupWidget.generated.h"

class USlider;
class UEditableTextBox;
class UButton;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSplitConfirmed, int32);
DECLARE_MULTICAST_DELEGATE(FOnSplitPopupClosed);

UCLASS()
class HEAVENBREACH_API UItemSplitPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	void SetupRange(int32 InMinCount, int32 InMaxCount);

	FOnSplitConfirmed OnSplitConfirmed;
	FOnSplitPopupClosed OnPopupClosed;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> QuantitySlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> QuantityText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

private:
	UFUNCTION()
	void HandleSliderChanged(float Value);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void UpdateDisplay();

	int32 MinCount = 1;
	int32 MaxCount = 1;
	int32 CurrentValue = 1;
};