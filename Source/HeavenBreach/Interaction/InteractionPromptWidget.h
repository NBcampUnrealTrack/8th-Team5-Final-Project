// Copyright HeavenBreach Team5. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class UTextBlock;

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

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;
};