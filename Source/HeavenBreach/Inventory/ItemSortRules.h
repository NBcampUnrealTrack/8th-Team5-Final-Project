#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.h"

namespace HBItemSort
{
	HEAVENBREACH_API int32 GetCategoryRank(const FDataTableRowHandle& InItemRow, const FItemViewData& InViewData);
}
