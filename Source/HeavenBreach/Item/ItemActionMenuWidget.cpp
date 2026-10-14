#include "Item/ItemActionMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UItemActionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (UseButton != nullptr)
	{
		UseButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleUseClicked);
	}
	if (SplitButton != nullptr)
	{
		SplitButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleSplitClicked);
	}
	if (InfoButton != nullptr)
	{
		InfoButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleInfoClicked);
	}
	if (DiscardButton != nullptr)
	{
		DiscardButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleDiscardClicked);
	}
	if (FavoriteButton != nullptr)
	{
		FavoriteButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleFavoriteClicked);
	}
	if (LockButton != nullptr)
	{
		LockButton->OnClicked.AddDynamic(this, &UItemActionMenuWidget::HandleLockClicked);
	}
}

void UItemActionMenuWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (!HasFocusedDescendants())
	{
		CloseMenu();
	}
}

FReply UItemActionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCloseRequested.Broadcast();
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UItemActionMenuWidget::CloseMenu()
{
	OnMenuClosed.Broadcast();
	RemoveFromParent();
}

void UItemActionMenuWidget::SetupForItem(bool bCanUse, bool bCanSplit, bool bIsFavorite)
{
	if (UseButton != nullptr)
	{
		UseButton->SetIsEnabled(bCanUse);
	}
	if (SplitButton != nullptr)
	{
		SplitButton->SetIsEnabled(bCanSplit);
	}
	if (FavoriteButtonText != nullptr)
	{
		if (bIsFavorite)
		{
			FavoriteButtonText->SetText(FText::FromString(TEXT("\uC990\uACA8\uCC3E\uAE30 \uD574\uC81C")));
		}
		else
		{
			FavoriteButtonText->SetText(FText::FromString(TEXT("\uC990\uACA8\uCC3E\uAE30")));
		}
	}
	}	

void UItemActionMenuWidget::SetUseButtonLabel(const FText& InLabel)
{
	if (UseButtonText != nullptr)
	{
		UseButtonText->SetText(InLabel);
	}
}

void UItemActionMenuWidget::HandleUseClicked()
{
	OnUseRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::HandleSplitClicked()
{
	OnSplitRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::HandleInfoClicked()
{
	OnInfoRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::HandleDiscardClicked()
{
	OnDiscardRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::HandleFavoriteClicked()
{
	OnFavoriteRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::HandleLockClicked()
{
	OnLockRequested.Broadcast();
	CloseMenu();
}

void UItemActionMenuWidget::SetupLock(bool bCanLock, bool bIsLocked)
{
	if (LockButton != nullptr)
	{
		LockButton->SetVisibility(bCanLock ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LockButtonText != nullptr)
	{
		LockButtonText->SetText(FText::FromString(bIsLocked ? TEXT("\uC7A0\uAE08 \uD574\uC81C") : TEXT("\uC7A0\uAE08")));
	}
	if (DiscardButton != nullptr && bIsLocked)
	{
		DiscardButton->SetIsEnabled(false);
	}
}