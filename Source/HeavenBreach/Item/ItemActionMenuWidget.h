#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemActionMenuWidget.generated.h"

class UButton;

DECLARE_MULTICAST_DELEGATE(FOnContextMenuUseRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuSplitRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuInfoRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuDiscardRequested);
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
	FOnContextMenuInfoRequested OnInfoRequested;
	FOnContextMenuDiscardRequested OnDiscardRequested;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SplitButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> InfoButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DiscardButton;

private:
	UFUNCTION()
	void HandleUseClicked();
	UFUNCTION()
	void HandleSplitClicked();
	UFUNCTION()
	void HandleInfoClicked();
	UFUNCTION()
	void HandleDiscardClicked();
};