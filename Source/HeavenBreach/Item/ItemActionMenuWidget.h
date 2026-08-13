#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemActionMenuWidget.generated.h"

class UButton;

DECLARE_MULTICAST_DELEGATE(FOnContextMenuUseRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuSplitRequested);

UCLASS()
class HEAVENBREACH_API UItemActionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	void SetupForItem(bool bCanUse, bool bCanSplit);

	FOnContextMenuUseRequested OnUseRequested;
	FOnContextMenuSplitRequested OnSplitRequested;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SplitButton;

private:
	UFUNCTION()
	void HandleUseClicked();

	UFUNCTION()
	void HandleSplitClicked();
};