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
DECLARE_MULTICAST_DELEGATE(FOnContextMenuLockRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuCloseRequested);
DECLARE_MULTICAST_DELEGATE(FOnContextMenuClosed);
UCLASS()

class HEAVENBREACH_API UItemActionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	void SetupForItem(bool bCanUse, bool bCanSplit, bool bIsFavorite);
	void SetUseButtonLabel(const FText& InLabel);
	void SetupLock(bool bCanLock, bool bIsLocked);
	FOnContextMenuUseRequested OnUseRequested;
	FOnContextMenuSplitRequested OnSplitRequested;
	FOnContextMenuInfoRequested OnInfoRequested;
	FOnContextMenuDiscardRequested OnDiscardRequested;
	FOnContextMenuFavoriteRequested OnFavoriteRequested;
	FOnContextMenuLockRequested OnLockRequested;
	FOnContextMenuCloseRequested OnCloseRequested;
	FOnContextMenuClosed OnMenuClosed;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SplitButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> InfoButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DiscardButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FavoriteButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FavoriteButtonText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UseButtonText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LockButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockButtonText;

private:
	void CloseMenu();

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
	UFUNCTION()
	void HandleLockClicked();
};