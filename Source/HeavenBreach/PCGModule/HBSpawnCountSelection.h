#pragma once

#include "CoreMinimal.h"
#include "HBSpawnCountSelection.generated.h"

UENUM(BlueprintType)
enum class EHBSpawnCountSelectionMode : uint8
{
	Uniform UMETA(DisplayName="Uniform (Min - Max)"),
	Weighted UMETA(DisplayName="Weighted")
};

USTRUCT(BlueprintType)
struct FHBSpawnCountWeight
{
	GENERATED_BODY()

	/** A count inside this group's inclusive MinCount / MaxCount range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn Count", meta=(ClampMin="0"))
	int32 Count = 0;

	/** Relative chance; zero disables this count. Omitted counts are also disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn Count", meta=(ClampMin="0"))
	int32 Weight = 1;
};

namespace HBSpawnCountSelection
{
	HEAVENBREACH_API bool Validate(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
		const TArray<FHBSpawnCountWeight>& Weights, TArray<FString>& OutErrors);
	HEAVENBREACH_API bool GetSelectableRange(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
		const TArray<FHBSpawnCountWeight>& Weights, int32& OutMinimum, int32& OutMaximum);
	/** Uniform mode keeps the original RandRange call, including its random-stream consumption. */
	HEAVENBREACH_API bool Select(int32 MinCount, int32 MaxCount, EHBSpawnCountSelectionMode Mode,
		const TArray<FHBSpawnCountWeight>& Weights, FRandomStream& Random, int32& OutCount, FString& OutError);
}
