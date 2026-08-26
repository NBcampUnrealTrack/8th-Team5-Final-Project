#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemInfoPopupWidget.generated.h"

class UTextBlock;
class UImage;

UCLASS()
class HEAVENBREACH_API UItemInfoPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void SetupInfo(const FString& InDisplayName, const FString& InDescription,
		bool bInIsRelic, const FString& InBenefitDescription, const FString& InRiskDescription,
		int32 InSellValue, UTexture2D* InIcon);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SellValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BenefitText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RiskText;
};