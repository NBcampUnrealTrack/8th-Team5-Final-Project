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
}

void UItemActionMenuWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (!HasFocusedDescendants())
	{
		RemoveFromParent();
	}
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

void UItemActionMenuWidget::HandleUseClicked()
{
	OnUseRequested.Broadcast();
	RemoveFromParent();
}

void UItemActionMenuWidget::HandleSplitClicked()
{
	OnSplitRequested.Broadcast();
	RemoveFromParent();
}

void UItemActionMenuWidget::HandleInfoClicked()
{
	OnInfoRequested.Broadcast();
	RemoveFromParent();
}

void UItemActionMenuWidget::HandleDiscardClicked()
{
	OnDiscardRequested.Broadcast();
	RemoveFromParent();
}

void UItemActionMenuWidget::HandleFavoriteClicked()
{
	OnFavoriteRequested.Broadcast();
	RemoveFromParent();
}