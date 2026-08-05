#include "InventoryTypes.h"

bool FItemInstance::IsValid() const
{
	return !ItemRow.IsNull() && InstanceId.IsValid() && StackCount > 0;
}

bool ResolveItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData)
{
	if (InRow.IsNull())
	{
		return false;
	}

	static const FString ContextString(TEXT("ResolveItemViewData"));
	const FMockItemDataRow* Row = InRow.GetRow<FMockItemDataRow>(ContextString);
	if (Row == nullptr)
	{
		return false;
	}

	OutViewData.GridSize = Row->GridSize;
	OutViewData.EquipSlotType = Row->EquipSlotType;
	OutViewData.Category = Row->Category;
	OutViewData.MaxStack = FMath::Max(1, Row->MaxStack);
	OutViewData.DisplayName = Row->DisplayName;
	OutViewData.Icon = Row->Icon;

	return true;
}

FIntPoint GetOccupiedSize(const FItemInstance& InItem, const FItemViewData& InViewData)
{
	if (InItem.bRotated)
	{
		return FIntPoint(InViewData.GridSize.Y, InViewData.GridSize.X);
	}

	return InViewData.GridSize;
}