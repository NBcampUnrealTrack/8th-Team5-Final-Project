#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemActionMenuWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnContextMenuUseRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuSplitRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuInfoRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuDiscardRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuFavoriteRequested);
UCLASS()

class HEAVENBREACH_API UItemActionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	void SetupForItem(bool bCanUse, bool bCanSplit, bool bIsFavorite);
	FOnContextMenuUseRequested OnUseRequested;
	FOnContextMenuSplitRequested OnSplitRequested;
	FOnContextMenuInfoRequested OnInfoRequested;
	FOnContextMenuDiscardRequested OnDiscardRequested;
	FOnContextMenuFavoriteRequested OnFavoriteRequested;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SplitButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> InfoButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DiscardButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FavoriteButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FavoriteButtonText;

private:
	UFUNCTION()
	void HandleUseClicked();
	UFUNCTION()
	void HandleSplitClicked();
	UFUNCTION()
	void HandleInfoClicked();
	UFUNCTION()
	void HandleDiscardClicked();
	UFUNCTION()
	void HandleFavoriteClicked();
};