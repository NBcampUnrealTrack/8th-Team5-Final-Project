#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InventoryTypes.h"
#include "ShopTypes.generated.h"

USTRUCT(BlueprintType)
struct FShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Shop")
	FDataTableRowHandle ItemRow;

	UPROPERTY(EditAnywhere, Category = "Shop")
	int32 Stock = -1;
};

USTRUCT(BlueprintType)
struct FShopEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FDataTableRowHandle ItemRow;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 BuyPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 Stock = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FItemViewData ViewData;
};

USTRUCT(BlueprintType)
struct FTradeBasketEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FDataTableRowHandle ItemRow;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 UnitPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FItemViewData ViewData;
};

UENUM(BlueprintType)
enum class ETradeMode : uint8
{
	Buy		UMETA(DisplayName = "Buy"),
	Sell	UMETA(DisplayName = "Sell")
};