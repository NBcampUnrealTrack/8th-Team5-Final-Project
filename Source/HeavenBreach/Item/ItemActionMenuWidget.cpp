#include "Item/ItemActionMenuWidget.h"
#include "Components/Button.h"

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
}

void UItemActionMenuWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (!HasFocusedDescendants())
	{
		RemoveFromParent();
	}
}

void UItemActionMenuWidget::SetupForItem(bool bCanUse, bool bCanSplit)
{
	if (UseButton != nullptr)
	{
		UseButton->SetIsEnabled(bCanUse);
	}
	if (SplitButton != nullptr)
	{
		SplitButton->SetIsEnabled(bCanSplit);
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