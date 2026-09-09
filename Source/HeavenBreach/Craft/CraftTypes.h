#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CraftTypes.generated.h"

UENUM(BlueprintType)
enum class ECraftMode : uint8
{
	Craft,
	Enhance
};

USTRUCT(BlueprintType)
struct FCraftIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDataTableRowHandle ItemRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 1))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FCraftRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString RecipeName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FCraftIngredient> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDataTableRowHandle ResultItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 1))
	int32 ResultCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0))
	int32 GoldCost = 0;
};