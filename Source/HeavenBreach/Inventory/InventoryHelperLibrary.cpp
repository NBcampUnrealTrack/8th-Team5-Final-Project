#include "InventoryHelperLibrary.h"

UTexture2D* UInventoryHelperLibrary::GetItemIcon(const FItemViewData& InViewData)
{
	return LoadItemIcon(InViewData.IconPath);
}