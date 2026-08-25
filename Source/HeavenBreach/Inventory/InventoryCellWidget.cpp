#include "InventoryCellWidget.h"

void UInventoryCellWidget::InitializeCell(FIntPoint InCellPosition)
{
	CellPosition = InCellPosition;
	VisualState = EInventoryCellVisualState::Default;
	BP_OnVisualStateChanged(VisualState);
}

void UInventoryCellWidget::SetVisualState(EInventoryCellVisualState InState)
{
	if (VisualState == InState)
	{
		return;
	}

	VisualState = InState;
	BP_OnVisualStateChanged(VisualState);
}
