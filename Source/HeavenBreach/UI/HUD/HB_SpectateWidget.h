#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_SpectateWidget.generated.h"

class UTextBlock;

// 관전 중 화면 아래에 "누구를 보고 있는지"를 띄운다. WBP 없이 C++로만 만든다.
UCLASS()
class HEAVENBREACH_API UHB_SpectateWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};
