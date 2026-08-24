#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryHelperLibrary.generated.h"

class UTexture2D;

UCLASS()
class HEAVENBREACH_API UInventoryHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static UTexture2D* GetItemIcon(const FItemViewData& InViewData);
};